package main

import (
	"log"
	"net"
	"encoding/json"
)

type Userd struct {
	conn net.Conn
	buf  []byte
}

type Request struct {

	Op int
	Hash0 []byte
	hash1 []byte
	Result int
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


func (u *Userd) Remove(hash string) bool {

	var z Request
	z.Op = 3
	z.Hash0 = []byte(hash)
	
	nb,err := json.Marshal(z)
	if err != nil {
		log.Printf("json marshal error - %v\n",err)
		return false
	}

	_,err = u.conn.Write(nb)
	if err != nil {
		log.Printf("check: %v\n",err)
		return false
	}

	n,err := u.conn.Read(u.buf)
	if err != nil {
		log.Printf("check: %v\n",err)
		return false
	}
	
	err = json.Unmarshal(u.buf[:n],&z)
	if err != nil {
		log.Printf("check: json unmarshal - %v\n",err)
		return false
	}

	
	return (z.Result == 1 && z.Op == 3)
}

func (u *Userd) Add(hash string) bool {

	var z Request
	z.Op = 2
	z.Hash0 = []byte(hash)
	
	nb,err := json.Marshal(z)
	if err != nil {
		log.Printf("json marshal error - %v\n",err)
		return false
	}

	_,err = u.conn.Write(nb)
	if err != nil {
		log.Printf("check: %v\n",err)
		return false
	}

	n,err := u.conn.Read(u.buf)
	if err != nil {
		log.Printf("check: %v\n",err)
		return false
	}
	
	err = json.Unmarshal(u.buf[:n],&z)
	if err != nil {
		log.Printf("check: json unmarshal - %v\n",err)
		return false
	}

	
	return (z.Result == 1 && z.Op == 2)
}


func (u *Userd) Check(hash string) bool {

	var z Request
	z.Op = 1
	z.Hash0 = []byte(hash)
	
	nb,err := json.Marshal(z)
	if err != nil {
		log.Printf("json marshal error - %v\n",err)
		return false
	}

	_,err = u.conn.Write(nb)
	if err != nil {
		log.Printf("check: %v\n",err)
		return false
	}

	n,err := u.conn.Read(u.buf)
	if err != nil {
		log.Printf("check: %v\n",err)
		return false
	}
	
	err = json.Unmarshal(u.buf[:n],&z)
	if err != nil {
		log.Printf("check: json unmarshal - %v\n",err)
		return false
	}
	
	return (z.Result == 1 && z.Op == 1)
}
		


