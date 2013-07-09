package main

import (
	"fmt"
	"log"
	"net"
	"strings"
)

type Userd struct {
	conn net.Conn
	buf  []byte
}

func NewUserd(serv string) *Userd {

	var u Userd
	conn, err := net.Dial("udp4", serv)
	u.conn = conn
	if err != nil {
		log.Printf("newuserd error : %v\n", err)
		return nil
	}
	u.buf = make([]byte, 512)
	return &u
}

func (u *Userd) Close() {

	u.conn.Close()
}

func (u *Userd) Remove(hash string) (bool, error) {

	request := fmt.Sprintf("removec>%s", hash)
	_, err := u.conn.Write([]byte(request))
	if err != nil {
		return false, err
	}

	n, err := u.conn.Read(u.buf)
	if err != nil {
		return false, err
	}

	rets := strings.Split(string(u.buf[:n]), ">")
	if len(rets) != 3 {
		return false, err
	}

	if rets[0] != "removec" {
		return false, nil
	}
	if rets[1] == hash {

		if rets[2] == "yes" {
			return true, nil
		}
	}

	return false, nil
}

func (u *Userd) Add(hash string) (bool, error) {

	request := fmt.Sprintf("addc>%s", hash)
	_, err := u.conn.Write([]byte(request))
	if err != nil {
		return false, err
	}

	n, err := u.conn.Read(u.buf)
	if err != nil {
		return false, err
	}

	rets := strings.Split(string(u.buf[:n]), ">")
	if len(rets) != 3 {
		return false, err
	}

	if rets[0] != "addc" {
		return false, nil
	}
	if rets[1] == hash {

		if rets[2] == "yes" {
			return true, nil
		}
	}

	return false, nil
}

func (u *Userd) Check(hash string) bool {

	request := fmt.Sprintf("check>%s", hash)
	_, err := u.conn.Write([]byte(request))
	if err != nil {
		log.Printf("check: %v\n", err)
		return false
	}

	n, err := u.conn.Read(u.buf)
	if err != nil {
		log.Printf("check: %v\n", err)
		return false
	}

	rets := strings.Split(string(u.buf[:n]), ">")
	if len(rets) != 3 {
		return false
	}

	if rets[0] != "check" {
		return false
	}

	if rets[1] == hash {

		if rets[2] == "yes" {
			return true
		}
	}

	return false
}
