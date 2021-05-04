// Command solc is a light go-based wrapper of the solc binary (written in c++) which provides a convenient mechanism for including and using solc from Go's tooling ecosystem.
package main

import "os"

func main() {
	os.Exit(run())
}
