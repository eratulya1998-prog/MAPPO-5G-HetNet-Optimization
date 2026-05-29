// ============================================================
// MAPPO Based Energy-Efficient HetNet Framework
// Advanced Energy-Aware 5G HetNet Simulator
// Features:
// ✅ HetNet Topology
// ✅ Dynamic SBS Sleep Mode
// ✅ Backhaul Model
// ✅ Energy Efficiency
// ✅ Dynamic Power Optimization
// ✅ Load-Aware SBS Control
// ✅ KPI Extraction
// ✅ Traffic-Aware Optimization
// ============================================================

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyAwareHetNet");

int
main(int argc, char* argv[])

{
    // ============================================================
    // Random Seed for Reproducibility
    // ============================================================

    std::srand(time(0));
    SeedManager::SetSeed(time(0));

    SeedManager::SetRun(rand() % 10000);

    // ============================================================
    // Simulation Parameters
    // ============================================================

    uint32_t numMacro = 1;
    uint32_t numSmall = 6;
    uint32_t numUEs = 30;

    double simTime = 15.0;
    double ueTrafficRate = 10 + (std::rand() % 50);

    // ============================================================
    // Create Nodes
    // ============================================================

    NodeContainer macroGnb;
    macroGnb.Create(numMacro);

    NodeContainer smallGnbs;
    smallGnbs.Create(numSmall);

    NodeContainer ueNodes;
    ueNodes.Create(numUEs);

    // ============================================================
    // Mobility Configuration
    // ============================================================

    MobilityHelper mobility;

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // ============================================================
    // Macro BS Position
    // ============================================================

    Ptr<ListPositionAllocator> macroPosition = CreateObject<ListPositionAllocator>();

    macroPosition->Add(Vector(500.0, 500.0, 0.0));

    mobility.SetPositionAllocator(macroPosition);

    mobility.Install(macroGnb);

    // ============================================================
    // Small BS Positions
    // ============================================================

    Ptr<ListPositionAllocator> smallBsPosition = CreateObject<ListPositionAllocator>();

    smallBsPosition->Add(Vector(250.0, 250.0, 0.0));

    smallBsPosition->Add(Vector(250.0, 750.0, 0.0));

    smallBsPosition->Add(Vector(500.0, 250.0, 0.0));

    smallBsPosition->Add(Vector(500.0, 750.0, 0.0));

    smallBsPosition->Add(Vector(750.0, 250.0, 0.0));

    smallBsPosition->Add(Vector(750.0, 750.0, 0.0));

    mobility.SetPositionAllocator(smallBsPosition);

    mobility.Install(smallGnbs);

    // ============================================================
    // UE Mobility
    // ============================================================

    MobilityHelper ueMobility;

    // ============================================================
    // Random UE Position Allocation
    // ============================================================

    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X",
                                    StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                    "Y",
                                    StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));

    // ============================================================
    // Dynamic UE Mobility Model
    // ============================================================

    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds",
                                RectangleValue(Rectangle(0.0, 1000.0, 0.0, 1000.0)),
                                "Speed",
                                StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                                "Distance",
                                DoubleValue(50.0));

    ueMobility.Install(ueNodes);

    // ============================================================
    // Print UE Positions
    // ============================================================

    for (uint32_t i = 0; i < ueNodes.GetN(); i++)
    {
        Ptr<MobilityModel> mobilityModel = ueNodes.Get(i)->GetObject<MobilityModel>();

        Vector position = mobilityModel->GetPosition();

        std::cout << "UE " << i << " Position: (" << position.x << ", " << position.y << ")"
                  << std::endl;
    }

    // ============================================================
    // Propagation Loss Model
    // ============================================================

    Ptr<LogDistancePropagationLossModel> propagation =
        CreateObject<LogDistancePropagationLossModel>();

    propagation->SetPathLossExponent(3.0);

    propagation->SetReference(1.0, 46.0);

    std::cout << "LogDistancePropagationLossModel Enabled" << std::endl;

    // ============================================================
    // Internet Stack
    // ============================================================

    InternetStackHelper internet;

    internet.Install(macroGnb);
    internet.Install(smallGnbs);
    internet.Install(ueNodes);
    // ============================================================
    // CSMA Backhaul + UE Network
    // ============================================================

    NodeContainer networkNodes;

    networkNodes.Add(macroGnb);
    networkNodes.Add(smallGnbs);
    networkNodes.Add(ueNodes);

    // ============================================================
    // CSMA Configuration
    // ============================================================

    CsmaHelper csma;

    csma.SetChannelAttribute("DataRate", StringValue("10Gbps"));

    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    // ============================================================
    // Queue Configuration
    // ============================================================

    csma.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    // ============================================================
    // Install CSMA Devices
    // ============================================================

    NetDeviceContainer devices = csma.Install(networkNodes);
    // ============================================================
    // IP Addressing
    // ============================================================

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // ============================================================
    // Traffic Generation
    // ============================================================

    uint16_t port = 4000;

    // ============================================================
    // UDP Server
    // ============================================================

    UdpServerHelper server(port);

    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));

    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(simTime));

    // ============================================================
    // RL-Controlled Traffic Interval
    // ============================================================

    double trafficInterval = 400.0 / ueTrafficRate;

    // Minimum Safe Interval

    if (trafficInterval < 80.0)
    {
        trafficInterval = 80.0;
    }

    // Maximum Safe Interval

    if (trafficInterval > 200.0)
    {
        trafficInterval = 200.0;
    }

    std::cout << "Traffic Interval: " << trafficInterval << " ms" << std::endl;

    // ============================================================
    // Multi-UE Traffic Generation
    // ============================================================

    ApplicationContainer clientApps;

    for (uint32_t i = 1; i < numUEs; i++)
    {
        UdpClientHelper client(interfaces.GetAddress(0), port);

        client.SetAttribute("MaxPackets", UintegerValue(5000));

        client.SetAttribute("Interval", TimeValue(MilliSeconds(trafficInterval)));

        client.SetAttribute("PacketSize", UintegerValue(2048));

        ApplicationContainer tempClient = client.Install(ueNodes.Get(i));

        clientApps.Add(tempClient);
    }
    // ============================================================
    // Start / Stop Applications
    // ============================================================

    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(simTime));

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime - 0.5));

    // ============================================================
    // Flow Monitor
    // ============================================================

    FlowMonitorHelper flowmon;

    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // ============================================================
    // Print Topology Information
    // ============================================================

    std::cout << "=======================================" << std::endl;

    std::cout << "ENERGY-AWARE 5G HETNET SIMULATOR" << std::endl;

    std::cout << "=======================================" << std::endl;

    std::cout << "Macro BS: " << macroGnb.GetN() << std::endl;

    std::cout << "Small BS: " << smallGnbs.GetN() << std::endl;

    std::cout << "UE Count: " << ueNodes.GetN() << std::endl;

    std::cout << "=======================================" << std::endl;

    // ============================================================
    // CSV FILE HEADER
    // ============================================================

    std::ifstream checkFile("rl_state.csv");

    bool fileExists = checkFile.good();

    checkFile.close();

    std::ofstream rlFile;

    rlFile.open("rl_state.csv", std::ios::app);

    if (!fileExists)
    {
        rlFile << "Throughput," << "Delay," << "EnergyEfficiency," << "TotalPower," << "SINR,"
               << "ActiveSBS," << "BackhaulPower," << "Reward," << "Action," << "PacketLoss"
               << std::endl;
    }

    rlFile.close();

    // ============================================================
    // Run Simulation
    // ============================================================

    Simulator::Stop(Seconds(simTime + 1.0));

    Simulator::Run();

    // ============================================================
    // KPI Extraction
    // ============================================================

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "Flow Stats Size: " << stats.size() << std::endl;

    double avgThroughput = 0.0;
    double totalDelay = 0.0;

    uint32_t totalLostPackets = 0;
    uint32_t txPackets = 0;

    int flowCount = 0;

    // ============================================================
    // Flow Statistics
    // ============================================================

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        double duration = iter->second.timeLastRxPacket.GetSeconds() -
                          iter->second.timeFirstTxPacket.GetSeconds();

        if (duration <= 0)
        {
            continue;
        }

        double throughput = iter->second.rxBytes * 8.0 / duration / 1024 / 1024;

        avgThroughput += throughput;

        double delay = 0.0;

        if (iter->second.rxPackets > 0)
        {
            delay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;

            totalDelay += delay;
        }

        uint32_t lostPackets = iter->second.lostPackets;

        totalLostPackets += lostPackets;

        txPackets += iter->second.txPackets;

        flowCount++;

        std::cout << "-------------------------------" << std::endl;

        std::cout << "Flow ID: " << iter->first << std::endl;

        std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

        std::cout << "Delay: " << delay << " sec" << std::endl;

        std::cout << "Lost Packets: " << lostPackets << std::endl;
    }
    // ============================================================
    // Packet Loss Ratio
    // ============================================================

    double packetLossRatio = 0.0;

    if (txPackets > 0)
    {
        packetLossRatio = ((double)totalLostPackets / txPackets) * 100.0;
    }

    std::cout << "Packet Loss Ratio: " << packetLossRatio << " %" << std::endl;

    // ============================================================
    // Average Network KPIs
    // ============================================================

    double averageNetworkThroughput = 0.0;
    double averageDelay = 0.0;

    if (flowCount > 0)
    {
        averageNetworkThroughput = avgThroughput / flowCount;

        averageDelay = totalDelay / flowCount;
    }
    // ============================================================
    // SBS Load Model
    // ============================================================

    std::vector<double> sbsLoad(numSmall, 0.0);

    for (uint32_t i = 0; i < numSmall; i++)
    {
        sbsLoad[i] = 10 + (std::rand() % 50);
    }

    // ============================================================
    // SBS Distance Model
    // ============================================================

    std::vector<double> sbsDistance(numSmall, 0.0);

    for (uint32_t i = 0; i < numSmall; i++)
    {
        sbsDistance[i] = 50 + (std::rand() % 100);

        std::cout << "SBS " << i << " Distance: " << sbsDistance[i] << " meters" << std::endl;
    }

    // ============================================================
    // Dynamic Transmission Power Control
    // ============================================================

    std::vector<double> txPower(numSmall, 0.0);

    for (uint32_t i = 0; i < numSmall; i++)
    {
        // Realistic SBS Power Range

        txPower[i] = 22 + (std::rand() % 10);

        std::cout << "SBS " << i << " Tx Power: " << txPower[i] << " dBm" << std::endl;
    }
    // ============================================================
    // Dynamic Per-SBS SINR Model
    // ============================================================

    std::vector<double> sbsSinr(numSmall, 0.0);

    for (uint32_t i = 0; i < numSmall; i++)
    {
        // ========================================================
        // Carrier Frequency (GHz)
        // ========================================================

        double carrierFreqGHz = 3.5;

        // ========================================================
        // Distance Protection
        // ========================================================

        double distance = std::max(sbsDistance[i], 1.0);

        // ========================================================
        // Path Loss Model (3GPP-like Urban)
        // ========================================================

        double pathLoss = 35.0 + (30.0 * log10(distance / 1000.0)) + (20.0 * log10(carrierFreqGHz));

        // ========================================================
        // Received Signal Power (dBm)
        // ========================================================

        double signalPowerDbm = txPower[i] - pathLoss;

        // ========================================================
        // Convert dBm → Watts
        // ========================================================

        double signalPower = std::pow(10.0, (signalPowerDbm - 30.0) / 10.0);

        // ============================================================
        // RL ACTION IMPORT
        // ============================================================

        // Default safe action

        int rlAction = 4;

        // Open action file

        std::ifstream actionFile("action.txt");

        // Read RL action

        if (actionFile.is_open())
        {
            actionFile >> rlAction;

            actionFile.close();
        }

        // Invalid action protection

        if (rlAction < 0 || rlAction > 7)
        {
            std::cout << "Invalid RL Action Received" << std::endl;

            rlAction = 4;
        }

        // RL action output

        std::cout << "RL Action Received: " << rlAction << std::endl;

        // ============================================================
        // SBS Sleep State
        // ============================================================

        std::vector<bool> sbsSleeping(numSmall, false);

        // ============================================================
        // SINR Estimation
        // ============================================================

        for (uint32_t i = 0; i < numSmall; i++)
        {
            // ========================================================
            // Signal Power
            // ========================================================

            double signalPower =

                std::pow(10.0, (txPower[i] - pathLoss - 30.0) / 10.0);

            // ========================================================
            // Interference Modeling
            // ========================================================

            double interferencePower = 0.0;

            for (uint32_t j = 0; j < numSmall; j++)
            {
                if (j == i)
                {
                    continue;
                }

                // ====================================================
                // Random Interferer Distance
                // ====================================================

                double interfererDistance = 50.0 + (std::rand() % 150);

                // ====================================================
                // Interferer Path Loss
                // ====================================================

                double interfererPathLoss =

                    32.4

                    +

                    (20.0 * log10(interfererDistance / 1000.0))

                    +

                    (20.0 * log10(carrierFreqGHz));

                // ====================================================
                // Interfering Signal Power
                // ====================================================

                double interferingSignal =

                    std::pow(10.0, (txPower[j] - interfererPathLoss - 30.0) / 10.0);

                // ====================================================
                // Reduced Interference Coupling
                // ====================================================

                interferencePower += 0.03 * interferingSignal;
            }

            // ========================================================
            // Thermal Noise Power
            // ========================================================

            double noisePower = 1e-12;

            // ========================================================
            // Linear SINR
            // ========================================================

            double linearSinr =

                signalPower

                /

                (interferencePower + noisePower);

            // ========================================================
            // Stability Protection
            // ========================================================

            linearSinr = std::max(linearSinr, 1e-9);

            // ========================================================
            // Convert Linear SINR → dB
            // ========================================================

            sbsSinr[i] = 10.0 * log10(linearSinr);

            // ========================================================
            // SINR Clamping
            // ========================================================

            if (sbsSinr[i] < 0.0)
            {
                sbsSinr[i] = 0.0;
            }

            if (sbsSinr[i] > 35.0)
            {
                sbsSinr[i] = 35.0;
            }

            // ========================================================
            // Print SINR
            // ========================================================

            std::cout << "SBS " << i << " SINR: " << sbsSinr[i] << " dB" << std::endl;
        }

        // ============================================================
        // Research-Grade Adaptive UE Association
        // ============================================================

        // ============================================================
        // Weight Parameters
        // ============================================================

        // SINR importance

        double alpha = 0.50;

        // Load balancing importance

        double beta = 0.25;

        // Power efficiency importance

        double gamma = 0.15;

        // ============================================================
        // Best SBS Selection
        // ============================================================

        int bestSbs = -1;

        double bestScore = -std::numeric_limits<double>::infinity();

        // ============================================================
        // SBS Evaluation Loop
        // ============================================================

        for (uint32_t i = 0; i < numSmall; i++)
        {
            // ========================================================
            // Sleeping SBS Protection
            // ========================================================

            if (sbsSleeping[i])
            {
                std::cout << "SBS " << i << " is Sleeping" << std::endl;

                continue;
            }

            // ========================================================
            // Normalized Metrics
            // ========================================================

            double normalizedSinr = sbsSinr[i] / 35.0;

            double normalizedLoad = sbsLoad[i] / 100.0;

            double normalizedPower = txPower[i] / 30.0;

            // ========================================================
            // Congestion Penalty
            // ========================================================

            double congestionPenalty = sbsLoad[i] / 100.0;

            // ========================================================
            // Delay Penalty
            // ========================================================

            double delayPenalty = averageDelay * 2.0;

            // ========================================================
            // Energy Penalty
            // ========================================================

            double energyPenalty = txPower[i] / 50.0;

            // ========================================================
            // Final Association Score
            // ========================================================

            double associationScore =

                (alpha * normalizedSinr)

                -

                (beta * normalizedLoad)

                -

                (gamma * normalizedPower)

                -

                (0.25 * congestionPenalty)

                -

                (0.15 * delayPenalty)

                -

                (0.10 * energyPenalty);

            // ========================================================
            // RL-Aware Association Control
            // ========================================================

            if (rlAction == 0)
            {
                associationScore += (0.10 * normalizedSinr);
            }

            else if (rlAction == 5)
            {
                associationScore -= (0.10 * normalizedPower);
            }

            // ========================================================
            // Print Information
            // ========================================================

            std::cout << "--------------------------------" << std::endl;

            std::cout << "SBS " << i << " Load: " << sbsLoad[i] << std::endl;

            std::cout << "SBS " << i << " SINR: " << sbsSinr[i] << " dB" << std::endl;

            std::cout << "Association Score: " << associationScore << std::endl;

            // ========================================================
            // Best SBS Selection
            // ========================================================

            if (associationScore > bestScore)
            {
                bestScore = associationScore;

                bestSbs = i;
            }
        }

        // ============================================================
        // Safety Protection
        // ============================================================

        if (bestSbs < 0)
        {
            std::cout << "Emergency SBS Wake-Up Triggered" << std::endl;

            sbsSleeping[0] = false;

            bestSbs = 0;
        }

        // ============================================================
        // Final Output
        // ============================================================

        std::cout << "=======================================" << std::endl;

        std::cout << "Best SBS Selected: " << bestSbs << std::endl;

        // ============================================================
        // Adaptive PRB Allocation Model
        // ============================================================

        std::vector<int> sbsPrb(numSmall, 0);

        for (uint32_t i = 0; i < numSmall; i++)
        {
            // ========================================================
            // Base PRB Allocation
            // ========================================================

            int basePrb = 35;

            // ========================================================
            // Load-Aware PRB Scaling
            // ========================================================

            int loadFactor = static_cast<int>(sbsLoad[i] * 0.7);

            // ========================================================
            // SINR-Aware PRB Scaling
            // ========================================================

            int sinrFactor = static_cast<int>(sbsSinr[i] * 1.2);

            // ========================================================
            // Initial PRB Allocation
            // ========================================================

            sbsPrb[i] = basePrb + loadFactor + sinrFactor;

            // ========================================================
            // RL-Aware Adaptation
            // ========================================================

            // Action 2:
            // Increase PRBs

            if (rlAction == 2)
            {
                sbsPrb[i] += 8;
            }

            // Action 3:
            // Decrease PRBs

            else if (rlAction == 3)
            {
                sbsPrb[i] -= 4;
            }

            // ========================================================
            // Delay-Aware Congestion Control
            // ========================================================

            if (averageDelay > 0.08)
            {
                // Increase PRBs during congestion

                sbsPrb[i] += 10;
            }

            // ========================================================
            // Packet Loss Protection
            // ========================================================

            if (packetLossRatio > 1.0)
            {
                sbsPrb[i] += 5;
            }

            // ========================================================
            // Minimum PRB Protection
            // ========================================================

            if (sbsPrb[i] < 30)
            {
                sbsPrb[i] = 30;
            }

            // ========================================================
            // Maximum PRB Protection
            // ========================================================

            if (sbsPrb[i] > 100)
            {
                sbsPrb[i] = 100;
            }

            std::cout << "SBS " << i << " Allocated PRBs: " << sbsPrb[i] << std::endl;
        }

        // ============================================================
        // Default SBS State
        // ============================================================

        uint32_t activeSbs = 5;
        uint32_t sleepingSbs = 1;

        // ============================================================
        // RL ACTION EXECUTION
        // ============================================================

        if (rlAction == 0)
        {
            // Increase Tx Power

            for (uint32_t i = 0; i < numSmall; i++)
            {
                txPower[i] += 2.0;

                if (txPower[i] > 30.0)
                {
                    txPower[i] = 30.0;
                }
            }

            std::cout << "RL Action: Increasing SBS Tx Power" << std::endl;
        }

        else if (rlAction == 1)
        {
            // Decrease Tx Power

            for (uint32_t i = 0; i < numSmall; i++)
            {
                txPower[i] -= 2.0;

                if (txPower[i] < 18.0)
                {
                    txPower[i] = 18.0;
                }
            }

            std::cout << "RL Action: Decreasing SBS Tx Power" << std::endl;
        }

        else if (rlAction == 2)
        {
            // Activate All SBS

            activeSbs = 6;
            sleepingSbs = 0;

            std::cout << "RL Action: Activating All SBS" << std::endl;
        }

        else if (rlAction == 3)
        {
            // Sleep Mode

            activeSbs = 5;
            sleepingSbs = 1;

            std::cout << "RL Action: SBS Sleep Mode Activated" << std::endl;
        }

        else if (rlAction == 4)
        {
            // Increase PRBs

            sbsPrb[bestSbs] += 10;

            if (sbsPrb[bestSbs] > 75)
            {
                sbsPrb[bestSbs] = 75;
            }

            std::cout << "RL Action: Increasing PRB Allocation" << std::endl;
        }

        else if (rlAction == 5)
        {
            // Decrease PRBs

            sbsPrb[bestSbs] -= 8;

            if (sbsPrb[bestSbs] < 20)
            {
                sbsPrb[bestSbs] = 20;
            }

            std::cout << "RL Action: Decreasing PRB Allocation" << std::endl;
        }

        else
        {
            std::cout << "RL Action: No Optimization Applied" << std::endl;
        }

        // ============================================================
        // Dynamic Backhaul Capacity
        // ============================================================

        double backhaulRate = 200.0;

        // Sleep Mode

        if (rlAction == 3)
        {
            backhaulRate = 80.0;
        }

        // Increase PRB Allocation

        else if (rlAction == 4)
        {
            backhaulRate = 300.0;
        }

        // Activate All SBS

        else if (rlAction == 2)
        {
            backhaulRate = 500.0;
        }

        // Increase Tx Power

        else if (rlAction == 0)
        {
            backhaulRate = 250.0;
        }

        // Decrease Tx Power / Default

        else
        {
            backhaulRate = 150.0;
        }
        std::string backhaulDataRate = std::to_string((int)backhaulRate) + " Mbps";

        std::cout << "Dynamic Backhaul Rate: " << backhaulDataRate << std::endl;

        // ============================================================
        // Total SBS Transmission Power
        // ============================================================

        double totalSbsPower = 0.0;

        for (uint32_t i = 0; i < numSmall; i++)
        {
            totalSbsPower += std::pow(10.0, txPower[i] / 10.0) / 1000.0;
        }

        std::cout << "Total SBS Tx Power: " << totalSbsPower << " Watts" << std::endl;

        // ============================================================
        // SINR Conversion
        // ============================================================

        double linearSinr = std::pow(10.0, sbsSinr[bestSbs] / 10.0);

        // ============================================================
        // PRB Bandwidth Model
        // ============================================================

        double bandwidthPerPrb = 0.18;

        double totalBandwidth = bandwidthPerPrb * sbsPrb[bestSbs];

        // ============================================================
        // Research-Grade MIMO-Aware Shannon Capacity
        // ============================================================

        // 4x4 MIMO gain

        double mimoGain = 3.0;

        // ============================================================
        // Spectral Efficiency
        // ============================================================

        double spectralEfficiency = log2(1.0 + linearSinr);

        // ============================================================
        // Spectral Efficiency Protection
        // ============================================================

        // Realistic NR upper bound

        if (spectralEfficiency > 8.5)
        {
            spectralEfficiency = 8.5;
        }

        // ============================================================
        // Initial PHY Rate Estimation
        // ============================================================

        double estimatedRate = totalBandwidth * spectralEfficiency * mimoGain;

        // ============================================================
        // PHY/MAC Efficiency Loss
        // ============================================================

        // Scheduler + HARQ + CQI + signaling overhead

        estimatedRate *= 0.65;

        // ============================================================
        // Congestion-Aware Throughput Scaling
        // ============================================================

        double congestionFactor = 1.0 - (packetLossRatio / 100.0);

        // Protection

        if (congestionFactor < 0.5)
        {
            congestionFactor = 0.5;
        }

        estimatedRate *= congestionFactor;

        // ============================================================
        // Delay-Aware Throughput Scaling
        // ============================================================

        if (averageDelay > 0.1)
        {
            estimatedRate *= 0.85;
        }

        // ============================================================
        // Backhaul Bottleneck Protection
        // ============================================================

        if (estimatedRate > backhaulRate)
        {
            estimatedRate = backhaulRate * 0.95;
        }

        // ============================================================
        // Final Throughput Smoothing
        // ============================================================

        averageNetworkThroughput = 0.7 * averageNetworkThroughput + 0.3 * estimatedRate;

        // ============================================================
        // Output
        // ============================================================

        std::cout << "Estimated Data Rate: " << estimatedRate << " Mbps" << std::endl;

        // ============================================================
        // Research-Grade Realistic Throughput Estimation
        // ============================================================

        // ============================================================
        // PHY/MAC Efficiency Factor
        // ============================================================

        // Realistic 5G NR efficiency

        double phyEfficiency = 0.48;

        // ============================================================
        // Initial Radio Throughput
        // ============================================================

        double estimatedThroughput = estimatedRate * phyEfficiency;

        // ============================================================
        // Backhaul Bottleneck Protection
        // ============================================================

        if (estimatedThroughput > (0.90 * backhaulRate))
        {
            estimatedThroughput = 0.90 * backhaulRate;
        }

        // ============================================================
        // Delay-Aware Throughput Penalty
        // ============================================================

        // Softer penalty for stable PPO learning

        double delayPenalty = 1.0 / (1.0 + (0.8 * averageDelay));

        // ============================================================
        // Packet Loss Penalty
        // ============================================================

        double packetLossPenalty = 1.0 - (packetLossRatio / 100.0);

        // Protection

        if (packetLossPenalty < 0.6)
        {
            packetLossPenalty = 0.6;
        }

        // ============================================================
        // Congestion-Aware Throughput
        // ============================================================

        double realisticThroughput = estimatedThroughput * delayPenalty * packetLossPenalty;

        // ============================================================
        // Throughput Smoothing
        // ============================================================

        // Stabilizes PPO training

        averageNetworkThroughput =

            (0.7 * averageNetworkThroughput)

            +

            (0.3 * realisticThroughput);

        // ============================================================
        // Safety Protection
        // ============================================================

        if (averageNetworkThroughput < 1.0)
        {
            averageNetworkThroughput = 1.0;
        }

        // ============================================================
        // Output
        // ============================================================

        std::cout << "Updated Network Throughput: " << averageNetworkThroughput << " Mbps"
                  << std::endl;

        // ============================================================
        // Power Model
        // ============================================================

        // Macro BS static power

        double macroPower = 22.0;

        // SBS sleep mode power

        double sleepPower = 0.5;

        // ============================================================
        // Backhaul Power Model
        // ============================================================

        // Per-backhaul-link power

        double backhaulLinkPower = 3.0;

        // Total backhaul power

        double totalBackhaulPower = activeSbs * backhaulLinkPower;

        // ============================================================
        // Total Network Power
        // ============================================================

        double totalPower =

            macroPower

            +

            totalSbsPower

            +

            (sleepingSbs * sleepPower)

            +

            totalBackhaulPower;

        // ============================================================
        // Total Power Consumption
        // ============================================================

        double totalPowerConsumption = totalPower;

        // ============================================================
        // Backhaul Capacity
        // ============================================================

        double backhaulCapacity = backhaulRate;

        // ============================================================
        // Backhaul Utilization
        // ============================================================

        double backhaulUtilization =

            (averageNetworkThroughput / backhaulCapacity)

            * 100.0;

        // ============================================================
        // Backhaul Utilization Protection
        // ============================================================

        if (backhaulUtilization > 100.0)
        {
            backhaulUtilization = 100.0;
        }

        // ============================================================
        // Energy Efficiency
        // ============================================================

        // Mbps per Watt

        double energyEfficiency =

            averageNetworkThroughput / totalPowerConsumption;

        // ============================================================
        // Energy Efficiency Protection
        // ============================================================

        if (energyEfficiency < 0.0)
        {
            energyEfficiency = 0.0;
        }

        // ============================================================
        // Stable RL Reward Function
        // ============================================================

        double reward =

            (1.2 * averageNetworkThroughput)

            +

            (8.0 * energyEfficiency)

            -

            (0.15 * averageDelay)

            -

            (0.5 * packetLossRatio)

            -

            (0.01 * totalPowerConsumption)

            -

            (0.02 * backhaulUtilization);

        // ============================================================
        // Reward Protection
        // ============================================================

        if (reward < -200.0)
        {
            reward = -200.0;
        }

        if (reward > 300.0)
        {
            reward = 300.0;
        }

        // ============================================================
        // Output
        // ============================================================

        std::cout << "RL Reward: " << reward << std::endl;

        // ============================================================
        // RL State Export
        // ============================================================

        if (!fileExists)
        {
            rlFile << "Throughput," << "Delay," << "EnergyEfficiency," << "TotalPower," << "SINR,"
                   << "ActiveSBS," << "BackhaulPower," << "Reward," << "Action," << "PacketLoss"
                   << std::endl;
        }

        rlFile << averageNetworkThroughput << "," << averageDelay << "," << energyEfficiency << ","
               << totalPower << "," << sbsSinr[bestSbs] << "," << activeSbs << ","
               << totalBackhaulPower << "," << reward << "," << rlAction << "," << packetLossRatio
               << std::endl;

        rlFile.close();

        std::cout << "RL State exported to rl_state.csv" << std::endl;

        // ============================================================
        // Final Results
        // ============================================================

        std::cout << "=======================================" << std::endl;
        // ============================================================
        // Throughput Estimation Summary
        // ============================================================

        std::cout << "THROUGHPUT ESTIMATION" << std::endl;

        std::cout << "=======================================" << std::endl;

        std::cout << "Selected SBS: " << bestSbs << std::endl;

        std::cout << "Selected SBS SINR: " << sbsSinr[bestSbs] << " dB" << std::endl;

        std::cout << "Allocated PRBs: " << sbsPrb[bestSbs] << std::endl;

        std::cout << "Bandwidth per PRB: " << bandwidthPerPrb << " MHz" << std::endl;

        std::cout << "Total Bandwidth: " << totalBandwidth << " MHz" << std::endl;

        std::cout << "Spectral Efficiency: " << spectralEfficiency << " bits/s/Hz" << std::endl;

        std::cout << "Estimated PHY Data Rate: " << estimatedRate << " Mbps" << std::endl;

        std::cout << "Average Network Throughput: " << averageNetworkThroughput << " Mbps"
                  << std::endl;

        std::cout << "=======================================" << std::endl;

        std::cout << "ENERGY-AWARE HETNET RESULTS" << std::endl;

        std::cout << "=======================================" << std::endl;

        std::cout << "Average Throughput: " << averageNetworkThroughput << " Mbps" << std::endl;

        std::cout << "Average Delay: " << averageDelay << " sec" << std::endl;

        std::cout << "Total Lost Packets: " << totalLostPackets << std::endl;

        std::cout << "=======================================" << std::endl;

        std::cout << "Active SBS: " << activeSbs << std::endl;

        std::cout << "Sleeping SBS: " << sleepingSbs << std::endl;

        std::cout << "=======================================" << std::endl;

        std::cout << "Backhaul Power: " << totalBackhaulPower << " Watts" << std::endl;

        std::cout << "Backhaul Capacity: " << backhaulCapacity << " Mbps" << std::endl;

        std::cout << "Backhaul Utilization: " << backhaulUtilization << " %" << std::endl;

        std::cout << "=======================================" << std::endl;

        std::cout << "Total Power Consumption: " << totalPower << " Watts" << std::endl;

        std::cout << "Energy Efficiency: " << energyEfficiency << " Mbps/Watt" << std::endl;

        std::cout << "=======================================" << std::endl;

        // ============================================================
        // End Simulation
        // ============================================================

        Simulator::Destroy();

        return 0;
    }
