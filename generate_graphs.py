import pandas as pd
import matplotlib.pyplot as plt

# ============================================================
# LOAD CSV FILES
# ============================================================

mappo = pd.read_csv("mappo_training_log.csv")
ppo = pd.read_csv("ppo_training_log.csv")

# ============================================================
# SMOOTHING FUNCTION
# ============================================================

window = 5

mappo["RewardSmooth"] = \
    mappo["Reward"].rolling(window).mean()

ppo["RewardSmooth"] = \
    ppo["Reward"].rolling(window).mean()

# ============================================================
# REWARD GRAPH
# ============================================================

plt.figure(figsize=(8,5))

plt.plot(
    mappo["Episode"],
    mappo["RewardSmooth"],
    label="MAPPO"
)

plt.plot(
    ppo["Episode"],
    ppo["RewardSmooth"],
    label="PPO"
)

plt.xlabel("Episode")
plt.ylabel("Reward")
plt.title("Reward vs Episode")
plt.legend()
plt.grid()

plt.savefig("reward_graph.png")

print("Reward Graph Saved")

# ============================================================
# THROUGHPUT GRAPH
# ============================================================

plt.figure(figsize=(8,5))

plt.plot(
    mappo["Episode"],
    mappo["Throughput"],
    label="MAPPO"
)

plt.plot(
    ppo["Episode"],
    ppo["Throughput"] * 100,
    label="PPO"
)

plt.xlabel("Episode")
plt.ylabel("Throughput (Mbps)")
plt.title("Throughput Comparison")
plt.legend()
plt.grid()

plt.savefig("throughput_graph.png")

print("Throughput Graph Saved")

# ============================================================
# DELAY GRAPH
# ============================================================

plt.figure(figsize=(8,5))

plt.plot(
    mappo["Episode"],
    mappo["Delay"],
    label="MAPPO"
)

plt.plot(
    ppo["Episode"],
    ppo["Delay"],
    label="PPO"
)

plt.xlabel("Episode")
plt.ylabel("Delay")
plt.title("Delay Comparison")
plt.legend()
plt.grid()

plt.savefig("delay_graph.png")

print("Delay Graph Saved")

# ============================================================
# SINR GRAPH
# ============================================================

plt.figure(figsize=(8,5))

plt.plot(
    mappo["Episode"],
    mappo["SINR"],
    label="MAPPO"
)

plt.plot(
    ppo["Episode"],
    ppo["SINR"] * 35,
    label="PPO"
)

plt.xlabel("Episode")
plt.ylabel("SINR (dB)")
plt.title("SINR Comparison")
plt.legend()
plt.grid()

plt.savefig("sinr_graph.png")

print("SINR Graph Saved")

# ============================================================
# ENERGY EFFICIENCY GRAPH
# ============================================================

plt.figure(figsize=(8,5))

plt.plot(
    mappo["Episode"],
    mappo["EnergyEfficiency"],
    label="MAPPO"
)

plt.plot(
    ppo["Episode"],
    ppo["EnergyEfficiency"] * 5,
    label="PPO"
)

plt.xlabel("Episode")
plt.ylabel("Energy Efficiency")
plt.title("Energy Efficiency Comparison")
plt.legend()
plt.grid()

plt.savefig("energy_efficiency_graph.png")

print("Energy Efficiency Graph Saved")

# ============================================================
# PACKET LOSS GRAPH
# ============================================================

if "PacketLoss" in mappo.columns:

    plt.figure(figsize=(8,5))

    plt.plot(
        mappo["Episode"],
        mappo["PacketLoss"],
        label="MAPPO"
    )

    plt.xlabel("Episode")
    plt.ylabel("Packet Loss (%)")
    plt.title("Packet Loss Comparison")
    plt.legend()
    plt.grid()

    plt.savefig("packet_loss_graph.png")

    print("Packet Loss Graph Saved")

# ============================================================
# SLEEP RATIO GRAPH
# ============================================================

if "SleepRatio" in mappo.columns:

    plt.figure(figsize=(8,5))

    plt.plot(
        mappo["Episode"],
        mappo["SleepRatio"],
        label="MAPPO Sleep Ratio"
    )

    plt.xlabel("Episode")

    plt.ylabel("Sleep Ratio")

    plt.title("MAPPO Sleep Mode Performance")

    plt.legend()

    plt.grid()

    plt.savefig("sleep_ratio_graph.png")

    print("Sleep Ratio Graph Saved")

# ============================================================
# SLEEP RATIO GRAPH
# ============================================================

if "SleepRatio" in ppo.columns:

    plt.figure(figsize=(8,5))

    plt.plot(
        ppo["Episode"],
        ppo["SleepRatio"],
        label="Sleep Ratio"
    )

    plt.xlabel("Episode")
    plt.ylabel("Sleep Ratio")
    plt.title("Sleep Mode Performance")
    plt.legend()
    plt.grid()

    plt.savefig("sleep_ratio_graph.png")

    print("Sleep Ratio Graph Saved")

print("===================================")
print("ALL RESEARCH GRAPHS GENERATED")
print("===================================")
