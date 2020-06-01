package main

import (
	"flag"
	"fmt"

	"./topologies"
)

func main() {
	var topo = flag.Int("topo", 0, "topology selector")
	var procType = flag.Int("procType", 0, "type of processor")
	var lambda = flag.Float64("lambda", 0.005, "lambda poisson interarrival")
	var mu = flag.Float64("mu", 0.02, "mu service rate") // default 50usec
	var genType = flag.Int("genType", 0, "type of generator")
	var duration = flag.Float64("duration", 10000000, "experiment duration")

	var warm_start = flag.Float64("warm_start", 1, "function warm start delay")
	var cold_start = flag.Float64("cold_start", 100000, "function cold start delay")
	var cold_start_ratio = flag.Float64("cold_start_ratio", 0.5, "function cold start ratio")

	flag.Parse()
	fmt.Printf("Selected topology: %v\n", *topo)

	if *topo == 0 {
		topologies.SingleQueue(*lambda, *mu, *duration, *genType, *procType, *warm_start, *cold_start, *cold_start_ratio)
	} else if *topo == 1 {
		topologies.MultiQueue(*lambda, *mu, *duration, *genType, *procType, *warm_start, *cold_start, *cold_start_ratio)
	} else {
		panic("Unknown topology")
	}
}
