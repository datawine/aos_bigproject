package topologies

import (
	"fmt"

	"../blocks"
	"../engine"
)

// SingleQueue implement a single-generator-multiprocessor topology with a single
// queue. Each processor just dequeues from this queue
func SingleQueue(lambda, mu, duration float64, genType, procType int, warm_start, cold_start, cold_start_ratio float64) {

	engine.InitSim()

	//Init the statistics
	stats := &blocks.AllKeeper{}
	stats.SetName("Main Stats")
	engine.InitStats(stats)

	// Add generator
	var g blocks.Generator
	if genType == 0 {
		g = blocks.NewMMRandGenerator(lambda, mu)
	} else if genType == 1 {
		g = blocks.NewMDRandGenerator(lambda, 1/mu)
	} else if genType == 2 {
		g = blocks.NewMBRandGenerator(lambda, 1, 10*(1/mu-0.9), 0.9)
	} else if genType == 3 {
		g = blocks.NewMBRandGenerator(lambda, 1, 1000*(1/mu-0.999), 0.999)
	}

	g.SetCreator(&blocks.SimpleReqCreator{})

	// Create queues
	q := blocks.NewQueue(1024)

	// Create processors

	if procType == 0 {
		for i := 0; i < cores; i++ {
			p := &blocks.RTCProcessor{}
			p.AddInQueue(q)
			p.SetReqDrain(stats)
			engine.RegisterActor(p)
		}
	} else if procType == 1 {
		p := blocks.NewPSProcessor()
		p.SetWorkerCount(cores)
		p.AddInQueue(q)
		p.SetReqDrain(stats)
		engine.RegisterActor(p)
	} else if procType == 2 {
		for i := 0; i < cores; i++ {
			p := blocks.NewSLProcessor(warm_start, cold_start, cold_start_ratio)
			p.AddInQueue(q)
			p.SetReqDrain(stats)
			engine.RegisterActor(p)
		}
	}

	g.AddOutQueue(q)

	// Register the generator
	engine.RegisterActor(g)

	fmt.Printf("Cores:%v\tservice_rate:%v\tinterarrival_rate:%v\trunning_time:%v\twarm_start:%v\tcold_start:%v\t" +
		"cold_start_ratio:%v\texec_time:%v\n", cores, mu, lambda, duration, warm_start, cold_start, cold_start_ratio, 1/mu)
	engine.Run(duration)
}
