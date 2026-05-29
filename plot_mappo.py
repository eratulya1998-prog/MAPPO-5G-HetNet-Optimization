import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv(
    "mappo_training_log.csv"
)

plt.figure(figsize=(10,6))

plt.plot(
    data["Episode"],
    data["Reward"]
)

plt.xlabel("Episode")

plt.ylabel("Reward")

plt.title("MAPPO Reward Convergence")

plt.grid(True)

plt.savefig(
    "mappo_reward_graph.png"
)

plt.show()
