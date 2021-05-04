package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"
)

func runPlatform(platform, version string) int {
	log.SetPrefix("solc ")
	bp, err := binaryPath(platform, version)
	if err != nil {
		fmt.Fprintln(os.Stderr, "solc: ", err)
		return -17
	}
	cmd := exec.Command(bp, os.Args[1:]...)

	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return -18
	}
	return cmd.ProcessState.ExitCode()
}
