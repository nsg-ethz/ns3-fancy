# PCAP REPLAY TRAFFIC ( alternatively you can use --Scaling=x10 to send faster)

#fancy
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesSystem=660 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTimeZoomingMs=200 --ProbingTimeTopEntriesMs=10 --Pipeline=true --PipelineBoost=true --CostType=1 --NumTopEntriesTraffic=10000 --FailDropRate=1 --NumTopFails=10 --TopFailType=Random --NumBottomFails=0 --BottomFailType=Random --InDirBase=tests/inputs/test --TraceSlice=0 --Seed=1 --OutDirBase=output/fancy-pcap-reply-traffic --TrafficType=PcapReplayTraffic --SwitchType=Fancy"
#Simulation Time: 1.6
#Bw(s1->s2): 1.29999Gbps
#Bw(s2->s1): 122.05Mbps
#
## Failure Detected(s1->s2)
#Time: 1.6
#Path: 25 45 41
#BF Indexes: 49307 33379 86985 65254
#Drops: 6
#Loss: 1
#BF Collisions: 1
#Real Collisions: 1
#Flows:
#102.102.102.102 231.176.160.0 5555 7777 6 0
#Failure number: 2
## End Failure Detected
#
#Bw(s1->s2): 1.74624Gbps
#Bw(s2->s1): 126.394Mbps
#Bw(s1->s2): 1.36998Gbps
#Bw(s2->s1): 122.05Mbps
#s2 Sim Time to send packets: 0.0160244
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesSystem=660 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTimeZoomingMs=50 --ProbingTimeTopEntriesMs=10 --Pipeline=true --PipelineBoost=true --CostType=1 --NumTopEntriesTraffic=10000 --FailDropRate=1 --NumTopFails=0 --TopFailType=Random --NumBottomFails=0 --BottomFailType=Random --InDirBase=tests/inputs/test --TraceSlice=0 --Seed=1 --OutDirBase=output/fancy-pcap-reply-traffic --TrafficType=PcapReplayTraffic --SwitchType=Fancy"

#loss radar
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesTraffic=10000 --FailDropRate=1 --NumTopFails=10 --TopFailType=Random --NumBottomFails=0 --BottomFailType=Random --InDirBase=tests/inputs/test --TraceSlice=0 --Seed=1 --OutDirBase=output/loss-radar-pcap-reply-traffic --TrafficType=PcapReplayTraffic --SwitchType=LossRadar --BatchTime=50 --NumberOfCells=1024 --NumberHashes=3"
# First detection
# Simulation Time: 1.1
#102.102.102.102 142.224.129.0 5555 7777 6 16466
#102.102.102.102 142.83.99.0 5555 7777 6 13960

./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesTraffic=10000 --FailDropRate=1 --NumTopFails=0 --TopFailType=Random --NumBottomFails=0 --BottomFailType=Random --InDirBase=tests/inputs/test --TraceSlice=0 --Seed=1 --OutDirBase=output/loss-radar-pcap-reply-traffic --TrafficType=PcapReplayTraffic --SwitchType=LossRadar --BatchTime=50 --NumberOfCells=1024 --NumberHashes=3"

# net seer
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesTraffic=10000 --FailDropRate=1 --NumTopFails=10 --TopFailType=Random --NumBottomFails=0 --BottomFailType=Random --InDirBase=tests/inputs/test --TraceSlice=0 --Seed=1 --OutDirBase=output/net-seer-pcap-reply-traffic --TrafficType=PcapReplayTraffic --SwitchType=NetSeer --NumberOfCells=1024"
#Simulation Time: 1
#Bw(s1->s2): 0bps
#Bw(s2->s1): 0bps
#EGRESS PACKET DROPPED AT (s1)
#102.102.102.102 232.23.227.0 5555 7777 6 249
#EVENT LOSS DETECTED AT (s1) with SEQ->249
#102.102.102.102 232.23.227.0 5555 7777 6 0

./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesTraffic=10000 --FailDropRate=1 --NumTopFails=0 --TopFailType=Random --NumBottomFails=0 --BottomFailType=Random --InDirBase=tests/inputs/test --TraceSlice=0 --Seed=1 --OutDirBase=output/net-seer-pcap-reply-traffic --TrafficType=PcapReplayTraffic --SwitchType=NetSeer --NumberOfCells=1024"


# SYNTHETIC TRAFFIC (stateless)

#fancy
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=1 --NumTopEntriesSystem=660 --FailDropRate=1 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTimeZoomingMs=50 --ProbingTimeTopEntriesMs=10 --Pipeline=true --PipelineBoost=true --CostType=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/fancy-synthetic-traffic --TrafficType=SyntheticTraffic --SwitchType=Fancy --SendRate=5Gbps --NumFlows=100 --NumDrops=10"
# First detection 
# Failure Detected(s1->s2)
#Time: 1.15
#Path: 65 68 15
#BF Indexes: 44569 17280 99483 63207
#Drops: 198
#Loss: 1
#BF Collisions: 1
#Real Collisions: 1
#Flows:
#102.102.102.102 102.102.102.102 59651 9266 6 7541
#Failure number: 1
## End Failure Detected
#
#
## Failure Detected(s1->s2)
#Time: 1.15
#Path: 21 122 44
#BF Indexes: 91931 11733 86876 69991
#Drops: 197
#Loss: 1
#BF Collisions: 1
#Real Collisions: 1
#Flows:
#102.102.102.102 102.102.102.102 19153 63732 6 7541
#Failure number: 2
# End Failure Detected

#loss radar
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=1  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/loss-radar-synthetic-traffic --TrafficType=SyntheticTraffic --SwitchType=LossRadar --BatchTime=50 --NumberOfCells=1024 --NumberHashes=3 --SendRate=5Gbps --NumFlows=100 --NumDrops=10"

# net seer
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=1  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/net-seer-synthetic-traffic --TrafficType=SyntheticTraffic --SwitchType=NetSeer --NumberOfCells=1024 --SendRate=5Gbps --NumFlows=100 --NumDrops=10"

# SIMPLE TRAFFIC (no tests for this?)
#fancy
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=0.5 --NumTopEntriesSystem=660 --FailDropRate=1 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTimeZoomingMs=50 --ProbingTimeTopEntriesMs=10 --Pipeline=true --PipelineBoost=true --CostType=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/fancy-synthetic-traffic --TrafficType=TestTraffic --SwitchType=Fancy --SendRate=100Mbps"

#loss radar
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=0.5  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/loss-radar-synthetic-traffic --TrafficType=TestTraffic --SwitchType=LossRadar --BatchTime=50 --NumberOfCells=1024 --NumberHashes=3 --SendRate=100Mbps"

# net seer
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=0.5  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/net-seer-synthetic-traffic --TrafficType=TestTraffic --SwitchType=NetSeer --NumberOfCells=1024 --SendRate=100Mbps"

# BIG NETWORK WIHTOUT NAT
#fancy
./waf --run  "main --DebugFlag=false --PcapEnabled=true --TrafficStart=1 --SendDuration=5 --NumTopEntriesSystem=660 --FailDropRate=1 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTimeZoomingMs=50 --ProbingTimeTopEntriesMs=10 --Pipeline=true --PipelineBoost=true --CostType=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/fancy-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=Fancy --SendRate=100Mbps --EnableNat=false"

#loss radar
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=5  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/loss-radar-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=LossRadar --BatchTime=50 --NumberOfCells=1024 --NumberHashes=3 --SendRate=100Mbps --EnableNat=false" 

# net seer
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=5  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/net-seer-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=NetSeer --NumberOfCells=1024 --SendRate=100Mbps --EnableNat=false"

# BIG NETWORK WIHT NAT

./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=2 --NumTopEntriesSystem=660 --FailDropRate=1 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTimeZoomingMs=50 --ProbingTimeTopEntriesMs=10 --Pipeline=true --PipelineBoost=true --CostType=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/fancy-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=Fancy --SendRate=100Mbps --EnableNat=true"

#loss radar
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=0.5  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/loss-radar-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=LossRadar --BatchTime=50 --NumberOfCells=1024 --NumberHashes=3 --SendRate=100Mbps --EnableNat=true"

# net seer
./waf --run  "main --DebugFlag=false --PcapEnabled=false --TrafficStart=1 --SendDuration=0.5  --FailDropRate=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/net-seer-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=NetSeer --NumberOfCells=1024 --SendRate=100Mbps --EnableNat=true"


# TEST THINGS

# This does not seem to work and reports more failures than it should, why I thouhgt I fixed this already. Internally we are starting 100 flows per sec. 

./waf --run  "main --DebugFlag=false --PcapEnabled=true --TrafficStart=1 --SendDuration=3 --NumTopEntriesSystem=660 --FailDropRate=0.2 --TreeDepth=3 --LayerSplit=2 --CounterWidth=128 --TreeEnabled=True --MaxCounterCollisions=2 --ProbingTim
eZoomingMs=100 --ProbingTimeTopEntriesMs=150 --Pipeline=true --PipelineBoost=true --CostType=1 --InDirBase=tests/inputs/test --Seed=1 --OutDirBase=output/fancy-synthetic-traffic --TrafficType=TraceBasedTraffic --SwitchType=Fancy --SendRate=100Mbps --EnableNat=true"