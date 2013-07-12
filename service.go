
/* userd/service.go */
package main

import (
	"crypto/rand"
	"log"
	"net"
	"encoding/json"
	"encoding/hex"
	"time"
	"github.com/ha/doozer"
	"os/signal"
	"os"
	"syscall"
	"io"
	"fmt"
	"flag"
	"strconv"
)



var (
	VERSION = "0"
	OP_CHECK = 1
	OP_ADD = 2
	OP_REMOVE = 3
)


func main() {

	var optName = flag.String("n",generateUID(),"name of the service")
	var optAddr = flag.String("a","localhost:9999","net for address")
	var optImport = flag.Int("i",0,"importance, lowest is most important")
	var optDoozerd = flag.String("d","loopback:8046","doozerd address, nil means no support")
	var optAttach = flag.Bool("s",true,"Setup on doozerd as soon as possible, if not use SIGHUP to do so at a latter stage")
	flag.Parse()

	var store Store
	store.Hashes = make(map[string]*Info,0)

	addr,err := net.ResolveUDPAddr("udp4",*optAddr)
	if err != nil {
		log.Fatalf("resolve UDP err - %v\n",err)
	}

	conn,err := net.ListenUDP("udp4",addr)
	if err != nil {
		log.Fatalf("listen UDP error - %v\n",err)
	}
	defer conn.Close()
	
	b := make([]byte,512)

	/* announce service to doozerd : */
	if len(*optDoozerd) > 0 {

		if *optAttach {
			
			announcement(*optName,*optAddr,*optDoozerd,*optImport)
		} else {

			c := make(chan os.Signal,1)
			signal.Notify(c,syscall.SIGHUP)
			go func(){ 	
		
				<- c
				signal.Stop(c)
				announcement(*optName,*optAddr,*optDoozerd,*optImport)
			}()
		}
	}


	/* main service loop */
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
		

		nb,err := json.Marshal(z)
		if err != nil {
			log.Printf("error json marshal - %v\n",err)
			continue
		}

		conn.WriteToUDP(nb,raddr)
		continue
	}

}

func announcement(name,addr,daddr string,importance int) {
	
	c := make(chan os.Signal,1)
	signal.Notify(c,os.Interrupt,syscall.SIGHUP)
	go func(){ 
		
		var srv Service
		srv.Name = name
		srv.Addr = addr
		srv.Impr = importance
		srv.announce(daddr)
		
		<- c
		log.Printf("Interrupt\n")
		
		srv.unannouce(daddr)	
		os.Exit(1)
	}()
}


type Service struct {

	Name string
	Addr string
	Impr int
	
	InfoRev int64
	AddrRev int64
	ImprRev int64
	VerRev int64
}

func (srv *Service) announce(doozerd string) {

	path := "/services/userd/" + srv.Name

	doozer, err := doozer.Dial(doozerd)
	defer doozer.Close()

	if err != nil {
		
		log.Fatalf("unable to connect to doozerd service - %v\n",err)
	}

	srv.InfoRev,err = doozer.Set(path + "/info",-1,[]byte("{__info__}"))
	if err != nil { goto error }

	srv.AddrRev,err = doozer.Set(path + "/addr",-1,[]byte(srv.Addr))
	if err != nil { goto error }

	srv.ImprRev,err = doozer.Set(path + "/importance",-1,[]byte(strconv.Itoa(srv.Impr)))
	if err != nil { goto error }
	
	srv.VerRev,err = doozer.Set(path + "/version",-1,[]byte(VERSION))
	if err != nil { goto error }

	log.Printf("announced service\n")
	return

error:
	log.Fatalf("unable to set service in doozerd - %v\n",err)
	
}

func (srv *Service) unannouce(doozerd string) {

	path := "/services/userd/" + srv.Name

	doozer,err := doozer.Dial(doozerd)
	defer doozer.Close()
	
	if err != nil {

		log.Fatalf("unable to connect to doozerd service - %v\n",err)
	}

	err = doozer.Del(path + "/info",srv.InfoRev)
	if err != nil {

		log.Printf("unable to delete service in doozerd - %v\n",err)
	}
	err = doozer.Del(path + "/addr",srv.AddrRev)
	if err != nil {

		log.Printf("unable to delete service in doozerd - %v\n",err)
	}
	err = doozer.Del(path + "/importance",srv.ImprRev)
	if err != nil {

		log.Printf("unable to delete service in doozerd - %v\n",err)
	}
	err = doozer.Del(path + "/version",srv.VerRev)
	if err != nil {

		log.Printf("unable to delete service in doozerd - %v\n",err)
	}

	log.Printf("unannounced service\n")
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


func generateUID() (string) {

	b := make([]byte, 8)
	n, err := io.ReadFull(rand.Reader, b)
	if n != len(b) || err != nil {
		return ""
	}

	s := hex.EncodeToString(b)
	return fmt.Sprintf("n16-%s-%s-%s",s[:4],s[4:8],s[8:])
}





