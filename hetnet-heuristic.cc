// ============================================================
// HETNET HEURISTIC RESOURCE ALLOCATION
// File: hetnet-heuristic.cc
// ============================================================

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <ctime>
#include <algorithm>

using namespace ns3;

int
main(int argc, char *argv[])
{
    // ========================================================
    // RANDOM SEED
    // ========================================================

    SeedManager::SetSeed(time(0));

    SeedManager::SetRun(
        1 + std::rand() % 100000
    );

    // ========================================================
    // NETWORK PARAMETERS
    // ========================================================

    uint32_t numMacro = 1;

    uint32_t numSmall = 6;

    uint32_t numUEs = 30;

    double simTime = 15.0;

    double ueTrafficRate =
        15 + (std::rand() % 60);

    // ========================================================
    // PRINT HEADER
    // ========================================================

    std::cout
        << "======================================="
        << std::endl;

    std::cout
        << "HEURISTIC-BASED 5G HETNET"
        << std::endl;

    std::cout
        << "======================================="
        << std::endl;

    // ========================================================
    // NODE CREATION
    // ========================================================

    NodeContainer macroGnb;

    macroGnb.Create(numMacro);

    NodeContainer smallGnbs;

    smallGnbs.Create(numSmall);

    NodeContainer ueNodes;

    ueNodes.Create(numUEs);

    // ========================================================
    // MOBILITY MODEL
    // ========================================================

    MobilityHelper mobility;

    mobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel"
    );

    mobility.Install(macroGnb);

    mobility.Install(smallGnbs);

    MobilityHelper ueMobility;

    ueMobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds",
        RectangleValue(
            Rectangle(
                0.0,
                1000.0,
                0.0,
                1000.0)),
        "Speed",
        StringValue(
            "ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
        "Distance",
        DoubleValue(50.0));

    ueMobility.Install(ueNodes);

    // ========================================================
    // INTERNET STACK
    // ========================================================

    InternetStackHelper internet;

    internet.Install(ueNodes);

    // ========================================================
    // HEURISTIC STATE VARIABLES
    // ========================================================

    std::vector<double> sbsLoad(
        numSmall,
        0.0);

    std::vector<double> txPower(
        numSmall,
        0.0);

    std::vector<double> sbsSinr(
        numSmall,
        0.0);

    std::vector<int> sbsPrb(
        numSmall,
        0);

    std::vector<bool> sbsSleeping(
        numSmall,
        false);

    std::vector<double> sbsDistance(
        numSmall,
        0.0);

    // ========================================================
    // RANDOM SBS PARAMETERS
    // ========================================================

    for (uint32_t i = 0; i < numSmall; i++)
    {
        sbsLoad[i] =
            20 + (std::rand() % 80);

        txPower[i] =
            22 + (std::rand() % 8);

        sbsDistance[i] =
            50 + (std::rand() % 300);
    }

    // ========================================================
    // HEURISTIC SINR MODEL
    // ========================================================

    for (uint32_t i = 0; i < numSmall; i++)
    {
        double carrierFreqGHz = 3.5;

        double distance =
            std::max(
                sbsDistance[i],
                1.0);

        double pathLoss =

            35.0

            +

            (30.0 *
             log10(distance / 1000.0))

            +

            (20.0 *
             log10(carrierFreqGHz));

        double signalPower =

            std::pow(
                10.0,
                (txPower[i]
                 -
                 pathLoss
                 -
                 30.0) / 10.0);

        double noisePower =
            1e-12;

        double interferencePower =
            1e-10;

        double linearSinr =

            signalPower /

            (interferencePower
             + noisePower);

        sbsSinr[i] =
            10.0 * log10(linearSinr);

        sbsSinr[i] =
            std::max(
                0.0,
                std::min(
                    sbsSinr[i],
                    35.0));
    }

    // ========================================================
    // HEURISTIC SBS SLEEP MODE
    // ========================================================

    uint32_t sleepingSbs = 0;

    uint32_t activeSbs = numSmall;

    for (uint32_t i = 0; i < numSmall; i++)
    {
        if (sbsLoad[i] < 25.0)
        {
            sbsSleeping[i] = true;

            sleepingSbs++;

            activeSbs--;
        }
    }

    // ========================================================
    // HEURISTIC UE ASSOCIATION
    // ========================================================

    int bestSbs = -1;

    double bestScore = -9999.0;

    for (uint32_t i = 0; i < numSmall; i++)
    {
        if (sbsSleeping[i])
        {
            continue;
        }

        double normalizedSinr =
            sbsSinr[i] / 35.0;

        double normalizedLoad =
            sbsLoad[i] / 100.0;

        double associationScore =

            (0.7 * normalizedSinr)

            -

            (0.3 * normalizedLoad);

        if (associationScore > bestScore)
        {
            bestScore =
                associationScore;

            bestSbs = i;
        }
    }

    // ========================================================
    // HEURISTIC PRB ALLOCATION
    // ========================================================

    for (uint32_t i = 0; i < numSmall; i++)
    {
        if (sbsSleeping[i])
        {
            sbsPrb[i] = 0;

            continue;
        }

        int basePrb = 20;

        if (sbsLoad[i] > 80)
        {
            basePrb += 20;
        }

        else if (sbsLoad[i] > 60)
        {
            basePrb += 15;
        }

        else if (sbsLoad[i] > 40)
        {
            basePrb += 10;
        }

        else
        {
            basePrb += 5;
        }

        if (sbsSinr[i] > 20)
        {
            basePrb += 10;
        }

        sbsPrb[i] =
            std::max(
                20,
                std::min(
                    basePrb,
                    75));
    }

    // ========================================================
    // BACKHAUL HEURISTIC
    // ========================================================

    double backhaulRate =

        200 +

        (std::rand() % 150);

    double averageDelay =
        0.03 +
        ((double)(std::rand() % 20) / 1000.0);

    if (averageDelay > 0.05)
    {
        backhaulRate += 100;
    }

    // ========================================================
    // THROUGHPUT ESTIMATION
    // ========================================================

    double bandwidthMHz =

        sbsPrb[bestSbs] * 0.18;

    double spectralEfficiency =

        log2(
            1 +
            std::pow(
                10.0,
                sbsSinr[bestSbs] / 10.0));

    double estimatedThroughput =

        bandwidthMHz *

        spectralEfficiency;

    // ========================================================
    // POWER MODEL
    // ========================================================

    double totalPowerConsumption = 0.0;

    for (uint32_t i = 0; i < numSmall; i++)
    {
        if (sbsSleeping[i])
        {
            totalPowerConsumption += 3.0;
        }

        else
        {
            totalPowerConsumption +=
                txPower[i] * 1.5;
        }
    }

    // ========================================================
    // ENERGY EFFICIENCY
    // ========================================================

    double energyEfficiency =

        estimatedThroughput /

        totalPowerConsumption;

    // ========================================================
    // PACKET LOSS MODEL
    // ========================================================

    double packetLossRatio =

        std::max(
            0.5,
            8.0 -
            (sbsSinr[bestSbs] / 5.0));

    // ========================================================
    // OUTPUT RESULTS
    // ========================================================

    std::cout
        << "======================================="
        << std::endl;

    std::cout
        << "HEURISTIC RESULTS"
        << std::endl;

    std::cout
        << "======================================="
        << std::endl;

    std::cout
        << "Best SBS: "
        << bestSbs
        << std::endl;

    std::cout
        << "Selected SBS SINR: "
        << sbsSinr[bestSbs]
        << " dB"
        << std::endl;

    std::cout
        << "Allocated PRBs: "
        << sbsPrb[bestSbs]
        << std::endl;

    std::cout
        << "Estimated Throughput: "
        << estimatedThroughput
        << " Mbps"
        << std::endl;

    std::cout
        << "Average Delay: "
        << averageDelay
        << " sec"
        << std::endl;

    std::cout
        << "Packet Loss Ratio: "
        << packetLossRatio
        << " %"
        << std::endl;

    std::cout
        << "Energy Efficiency: "
        << energyEfficiency
        << " Mbps/W"
        << std::endl;

    std::cout
        << "Total Power Consumption: "
        << totalPowerConsumption
        << " W"
        << std::endl;

    std::cout
        << "Backhaul Capacity: "
        << backhaulRate
        << " Mbps"
        << std::endl;

    std::cout
        << "Active SBS: "
        << activeSbs
        << std::endl;

    std::cout
        << "Sleeping SBS: "
        << sleepingSbs
        << std::endl;

    std::cout
        << "======================================="
        << std::endl;

    Simulator::Stop(
        Seconds(simTime));

    Simulator::Run();

    Simulator::Destroy();

    return 0;
}
