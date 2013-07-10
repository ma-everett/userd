
/* userd/service.go */
package main

import (
	"log"
	"net"
	"encoding/json"
	"time"
)

var (
	OP_CHECK = 1
	OP_ADD = 2
	OP_REMOVE = 3
)


func main() {

	var store Store
	store.Hashes = make(map[string]*Info,0)

	addr,err := net.ResolveUDPAddr("udp4","localhost:9999")
	if err != nil {
		log.Fatalf("resolve UDP err - %v\n",err)
	}

	conn,err := net.ListenUDP("udp4",addr)
	if err != nil {
		log.Fatalf("listen UDP error - %v\n",err)
	}
	defer conn.Close()
	
	b := make([]byte,512)

	for {		
		n,raddr,err := conn.ReadFromUDP(b)
		if err != nil {
			log.Fatalf("error reading from UDP - %v\n",err)
		}
		now := time.Now()

		var z Request
		err = json.Unmarshal(b[0:n],&z)
		if err != nil {
			log.Printf("error json unmarshal - %v\n",err)
			continue
		}

		log.Printf("%d %s\n",z.Op,string(z.Hash0))

		z.Result = 0

		if z.Op == OP_CHECK { /*CHECK*/

			info := store.Hashes[ string(z.Hash0) ]
			if info != nil {
				info.Last = now
				z.Result = 1
			}
			
		} else if z.Op == OP_ADD {

			store.Hashes[ string(z.Hash0) ] = &Info{now,now} 
			z.Result = 1
		} else if z.Op == OP_REMOVE {

			info := store.Hashes[ string(z.Hash0) ]
			if info != nil {

				store.Hashes[ string(z.Hash0) ] = nil
				z.Result = 1
			}
		}
		

		/* just send back an _no_ */
		nb,err := json.Marshal(z)
		if err != nil {
			log.Printf("error json marshal - %v\n",err)
			continue
		}

		conn.WriteToUDP(nb,raddr)
		continue
	}

}

type Info struct {

	Set time.Time
	Last time.Time
}

type Store struct {

	Hashes map[string]*Info
}

type Request struct {

	Op int
	Hash0 []byte
	Hash1 []byte
	Result int
}


