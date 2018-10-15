package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"os"

	"golang.org/x/crypto/ssh"
)

func main() {
	client, err := ssh.Dial("tcp", "192.168.10.174:22", &ssh.ClientConfig{
		User:            "root",
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
		Auth: []ssh.AuthMethod{
			ssh.Password("obelisk"),
		},
	})
	if err != nil {
		log.Fatal(err)
	}

	content, err := ioutil.ReadFile(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}

	session, err := client.NewSession()
	if err != nil {
		log.Fatal(err)
	}
	defer session.Close()
	go func() {
		w, _ := session.StdinPipe()
		defer w.Close()
		fmt.Fprintln(w, "C0644", len(content), os.Args[1])
		w.Write(content)
		fmt.Fprint(w, "\x00") // transfer end with \x00
	}()
	if err := session.Run("/usr/bin/scp -t /tmp"); err != nil {
		log.Fatal(err)
	}
}
