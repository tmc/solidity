package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path/filepath"
)

type version = string
type platform = string

type release struct {
	url    string
	sha256 string
}

var latestRelease version = "v0.8.4"

var releases = map[version]map[platform]release{
	latestRelease: {
		"darwin": {
			url:    "https://github.com/ethereum/solidity/releases/download/v0.8.4/solc-macos",
			sha256: "4f6f2e6942a09051bbbc850d4fa9b0d907749612cb5db58cac0c87745435070f",
		},
		"linux": {
			url:    "https://github.com/ethereum/solidity/releases/download/v0.8.4/solc-static-linux",
			sha256: "f7115ccaf11899dcf3aaa888949f8614421f2d10af65a74870bcfd67010da7f8",
		},
		"windows": {
			url:    "https://github.com/ethereum/solidity/releases/download/v0.8.4/solc-windows.exe",
			sha256: "6bd9bb0a601e791692358037b5558deba15f970d82de56d030c6eab0d1e28d67",
		},
	},
}

func getRelease(p platform, v version) (release, error) {
	rp, ok := releases[v]
	if !ok {
		return release{}, fmt.Errorf("version %v is invalid", v)
	}
	return rp[p], nil
}

func binaryPath(p platform, v version) (string, error) {
	cacheDir, err := os.UserCacheDir()
	if err != nil {
		return "", err
	}
	r, err := getRelease(p, v)
	if err != nil {
		return "", err
	}
	binaryName := "solc"
	if p == "windows" {
		binaryName = "solc.exe"
	}
	binaryPath := filepath.Join(cacheDir, "solc", r.sha256, binaryName)
	return binaryPath, ensureBinaryPath(r, binaryPath)
}

func ensureBinaryPath(r release, binaryPath string) error {
	_, err := os.Stat(binaryPath)
	if os.IsNotExist(err) {
		err = downloadRelease(r, binaryPath)
		if err != nil {
			return err
		}
	}
	hasher := sha256.New()
	s, err := ioutil.ReadFile(binaryPath)
	hasher.Write(s)
	if err != nil {
		return err
	}
	got := hex.EncodeToString(hasher.Sum(nil))
	if r.sha256 != got {
		return fmt.Errorf("solc shasum failed: got %v, want %v", got, r.sha256)
	}

	return nil
}

func downloadRelease(r release, path string) error {
	dir := filepath.Dir(path)
	err := os.MkdirAll(dir, 0755)
	if err != nil {
		return err
	}
	log.Println("downloading", r.url)
	resp, err := http.Get(r.url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0755)
	if err != nil {
		return err
	}
	_, err = f.ReadFrom(resp.Body)
	return err
}
