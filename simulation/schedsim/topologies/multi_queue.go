package topologies

import (
	"fmt"

	"../blocks"
	"../engine"
)

// MultiQueue describes a single-generator-multi-processor topology where every
// processor has its own incoming queue
func MultiQueue(lambda, mu, duration float64, genType, procType int, warm_start, cold_start, cold_start_ratio float64) {

	engine.InitSim()

	//Init the statistics
	//stats := blocks.NewBookKeeper()
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
	fastQueues := make([]engine.QueueInterface, cores)
	for i := range fastQueues {
		fastQueues[i] = blocks.NewQueue(1024)
	}

	// Create processors
	processors := make([]blocks.Processor, cores)

	// first the slow cores
	for i := 0; i < cores; i++ {
		if procType == 0 {
			processors[i] = &blocks.RTCProcessor{}
		} else if procType == 1 {
			processors[i] = blocks.NewPSProcessor()
		} else if procType == 2 {
			processors[i] = blocks.NewSLProcessor(warm_start, cold_start, cold_start_ratio)
		}
	}

	// Connect the fast queues
	for i, q := range fastQueues {
		g.AddOutQueue(q)
		processors[i].AddInQueue(q)
	}

	// Add the stats and register processors
	for _, p := range processors {
		p.SetReqDrain(stats)
		engine.RegisterActor(p)
	}

	// Register the generator
	engine.RegisterActor(g)

	fmt.Printf("Cores:%v\tservice_rate:%v\tinterarrival_rate:%v\trunning_time:%v\twarm_start:%v\tcold_start:%v\t" +
		"cold_start_ratio:%v\texec_time:%v\n", cores, mu, lambda, duration, warm_start, cold_start, cold_start_ratio, 1/mu)
	engine.Run(duration)
}
