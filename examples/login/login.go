package main

import (
	"bytes"
	"crypto/rand"
	"crypto/sha512"
	"encoding/hex"
	"fmt"
	"html/template"
	"io"
	"log"
	"net/http"
	"time"
)

var (
	gch       = make(chan *State, 1)
	guserdch  = make(chan *Userd, 1)
	templates = template.Must(template.ParseFiles("login.html", "home.html", "stats.html"))
)

func main() {

	var state State
	state.Members = make(map[string]*Member, 0)
	state.Sessions = make(map[string]*Session, 0)
	state.Salt = "--salt--"
	state.Stats.Address = "localhost:9999"
	state.Stats.Session = nil

	fn := func() { gch <- &state }
	go fn()

	fn = func() {
		u := NewUserd(state.Stats.Address)
		if u == nil {
			log.Fatal("unable to connect to userd")
		}
		guserdch <- u
	}

	go fn()

	http.HandleFunc("/user/stats/", makeProtectedHandler(statsHandler))
	http.HandleFunc("/user/logout/", makeProtectedHandler(logoutHandler))
	http.HandleFunc("/user/home/", makeProtectedHandler(homeHandler))
	http.HandleFunc("/user/cancel/", makeProtectedHandler(cancelMembershipHandler))

	http.HandleFunc("/user/login/", loginHandler)
	http.HandleFunc("/user/new/", newMembershipHandler)

	http.HandleFunc("/css/style.css", styleHandler)
	http.HandleFunc("/", rootHandler)

	log.Fatal(http.ListenAndServe(":7070", nil))
}

func makeProtectedHandler(fn func(http.ResponseWriter, *http.Request, *Session)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		session := checkAndTouchSession(r)
		if session == nil {

			sessionTimeoutHandler(w, r)
			return
		}
		fn(w, r, session)
	}
}

func generateRandomBytes(length int) ([]byte, error) {

	b := make([]byte, length)
	n, err := io.ReadFull(rand.Reader, b)
	if n != len(b) || err != nil {

		return nil, err
	}

	return b, nil
}

func encodeSessionToken() string {

	b, err := generateRandomBytes(32)
	if err != nil {
		log.Printf("error generating random bytes - %v\n", err)
		return ""
	}

	st := hex.EncodeToString(b)
	offset := len(st) / 2
	return fmt.Sprintf("s%st%s", st[1:offset], st[offset+1:])
}

func validSessionToken(st string) bool {

	return (len(st) == 64 && st[0] == 's' && st[len(st)/2] == 't')
}

type Member struct {
	Username string
	Hash     []byte
	HashHR   string
	Signup   time.Time
}

func (m *Member) CheckPassword(password, salt string) bool {

	hash := sha512.New()
	hash.Write([]byte(m.Username + salt + password))
	h := hash.Sum(nil)

	return bytes.Equal(m.Hash, h)
}

func NewMember(username, password, salt string) *Member {

	log.Printf("newmember: %s\n", username)

	var m Member
	m.Signup = time.Now()
	m.Username = username

	hash := sha512.New()
	hash.Write([]byte(username + salt + password))
	m.Hash = hash.Sum(nil)
	m.HashHR = hex.EncodeToString(m.Hash)

	return &m
}

type Session struct {
	User           *Member
	SessionToken   []byte
	SessionTokenHR string
	Login          time.Time
	Touch          int
	revokech       chan bool
	/* MAYBE: add timeout here */
}

func NewSession(user *Member) *Session {

	/* NOTE: prefect moment to add to a list of sessions */

	var s Session
	s.User = user
	s.SessionTokenHR = encodeSessionToken()

	s.Login = time.Now()
	s.Touch = 0
	s.revokech = make(chan bool, 1)

	log.Printf("newsession for %s, session token = [%s]:%d\n", user.Username,
		s.SessionTokenHR, len(s.SessionTokenHR))

	return &s
}

type Stats struct {
	Session  *Session
	Duration time.Duration
	Address  string
	Hashes   int
	Users    int
	Failed   int
	Success  int
}

type State struct {
	Stats Stats

	Members  map[string]*Member
	Sessions map[string]*Session

	Salt string
}

func (s *State) AddMember(mem *Member) {

	s.Members[string(mem.Hash)] = mem
}

func (s *State) RemoveMember(mem *Member) {

	s.Members[string(mem.Hash)] = nil
}

func (s *State) AddSession(sn *Session) {

	s.Sessions[string(sn.SessionTokenHR)] = sn
}

func (s *State) RemoveSession(sn *Session) {

	s.Sessions[string(sn.SessionTokenHR)] = nil
}
func checkSession(r *http.Request) bool {

	q := r.URL.Query()
	st := q.Get("st")
	if validSessionToken(st) == false {
		return false
	}
	return true
}

func checkAndTouchSession(r *http.Request) *Session {

	q := r.URL.Query()
	st := q.Get("st")

	if validSessionToken(st) == false {

		log.Printf("invalid sessiontoken [%v]:%d\n", st, len(st))
		return nil
	}

	state := <-gch
	session := state.Sessions[st]
	if session != nil {

		session.Touch++
	}
	gch <- state
	return session
}

func render(w http.ResponseWriter, tmpl string, s *Session) {
	err := templates.ExecuteTemplate(w, tmpl+".html", s)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
}

func renderLogin(w http.ResponseWriter, msg string) {
	err := templates.ExecuteTemplate(w, "login.html", msg)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
}

func renderStats(w http.ResponseWriter, stats *Stats) {
	err := templates.ExecuteTemplate(w, "stats.html", stats)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
}

func logoutTask(session *Session, msg string) {

	state := <-gch
	log.Printf(msg)
	state.RemoveSession(session)
	gch <- state
}

func AutoLogout(session *Session) {

	var touch int
	touch = -1

	for touch < session.Touch {

		touch = session.Touch
		timer := time.NewTimer(2 * time.Minute)

		select {
		case <-timer.C:
			continue
		case revoke := <-session.revokech:
			if revoke {
				now := time.Now()
				go logoutTask(session,
					fmt.Sprintf("autologout : revoke on %s, %v\n", session.User.Username,
						now.Sub(session.Login)))

				return
			}
		}
	}

	now := time.Now()
	go logoutTask(session,
		fmt.Sprintf("autologout : timeout on %s, %v\n", session.User.Username, now.Sub(session.Login)))
}

func rootHandler(w http.ResponseWriter, r *http.Request) {

	renderLogin(w, "Please login.")
}

/* process post form for login */
func loginHandler(w http.ResponseWriter, r *http.Request) {

	username := r.FormValue("username")
	password := r.FormValue("password")

	if len(username) <= 0 || len(password) <= 0 {

		log.Printf("login: invalid username or/and password\n")
		failedLoginHandler(w, r)
		return
	}

	state := <-gch

	h := sha512.New()
	h.Write([]byte(username + state.Salt + password))
	hash := h.Sum(nil)

	u := <-guserdch

	if u.Check(hex.EncodeToString(hash)) == false {

		state.Stats.Failed++
		guserdch <- u
		gch <- state
		log.Printf("login: user %s does not exist at userd\n", username)
		failedLoginHandler(w, r)
		return
	}

	guserdch <- u

	user := state.Members[string(hash)]
	if user == nil {

		state.Stats.Failed++
		gch <- state
		log.Printf("login: user %s does not exist\n", username)
		failedLoginHandler(w, r)
		return
	}

	/* create a new session */
	session := NewSession(user)

	state.AddSession(session)
	state.Stats.Success++
	gch <- state

	go AutoLogout(session) /* in order to auto timeout the session */

	url := fmt.Sprintf("/user/home/?st=%s", session.SessionTokenHR)
	http.Redirect(w, r, url, http.StatusFound)
}

/* process logout request, get */
func logoutHandler(w http.ResponseWriter, r *http.Request, session *Session) {

	session.revokech <- true
	afterLogoutHandler(w, r)
}

/* generate home screen for user */
func homeHandler(w http.ResponseWriter, r *http.Request, session *Session) {

	render(w, "home", session)
}

func removeFromUserdTask(hash string) {

	u := <-guserdch
	b, _ := u.Remove(hash)
	if b == false {
		log.Printf("failed to remove hash [%s] from userd\n", hash)
	}
	guserdch <- u
}

/* cancel membership for user, remove details */
func cancelMembershipHandler(w http.ResponseWriter, r *http.Request, session *Session) {

	password := r.FormValue("password")

	if len(password) <= 0 {

		http.Redirect(w, r, fmt.Sprintf("/user/home/?st=%s", session.SessionTokenHR), http.StatusFound)
		return
	}

	state := <-gch

	if session.User != nil && session.User.CheckPassword(password, state.Salt) {

		log.Printf("removing member %s\n", session.User.Username)
		state.RemoveMember(session.User)

		go removeFromUserdTask(session.User.HashHR)

		gch <- state
		afterCancelHandler(w, r)
		return
	}

	gch <- state /* yield */

	http.Redirect(w, r, fmt.Sprintf("/user/home/?st=%s", session.SessionTokenHR), http.StatusFound)
}

/* new membership for user, add details */
func newMembershipHandler(w http.ResponseWriter, r *http.Request) {

	username := r.FormValue("username")
	password_0 := r.FormValue("password_0")
	password_1 := r.FormValue("password_1")

	if len(username) <= 0 || len(password_0) <= 0 {

		log.Printf("new : username or/and password invalid\n")
		renderLogin(w, "invalid username and/or password")
		return
	}

	if password_0 != password_1 {

		log.Printf("new : passwords do not match\n")
		renderLogin(w, "Unable to create new user as supplied passwords do not match")
		return
	}

	state := <-gch

	/* first check if user already exists */
	for _, m := range state.Members {

		/* TODO, uniform and check all usernames, tolower etc*/
		if m.Username == username {

			log.Printf("new : %s already exists\n", username)
			gch <- state
			renderLogin(w, fmt.Sprintf("%s already exists, please choose another username", username))
			return
		}
	}

	member := NewMember(username, password_0, state.Salt)
	state.AddMember(member)

	gch <- state

	u := <-guserdch
	u.Add(member.HashHR)
	guserdch <- u

	afterNewHandler(w, r)
}

func statsHandler(w http.ResponseWriter, r *http.Request, session *Session) {

	var stats Stats
	state := <-gch
	stats = state.Stats
	stats.Users = len(state.Sessions)
	gch <- state

	now := time.Now()
	stats.Duration = now.Sub(session.Login)
	stats.Session = session

	renderStats(w, &stats)
}

func failedLoginHandler(w http.ResponseWriter, r *http.Request) {

	renderLogin(w, "Unknown username and/or password, please try again.")
}

func afterLogoutHandler(w http.ResponseWriter, r *http.Request) {

	renderLogin(w, "You have successfully been logged out. See you later.")
}

func afterCancelHandler(w http.ResponseWriter, r *http.Request) {

	renderLogin(w, "You're membership has been successfully cancelled. You will be missed.")
}

func afterNewHandler(w http.ResponseWriter, r *http.Request) {

	renderLogin(w, "We have minted for you a shiny new membership. Please login to try it out.")
}

func sessionTimeoutHandler(w http.ResponseWriter, r *http.Request) {

	renderLogin(w, "You're session has timed out or is invalid, please login to continue.")
}

func styleHandler(w http.ResponseWriter, r *http.Request) {

	http.ServeFile(w, r, "style.css")
}
