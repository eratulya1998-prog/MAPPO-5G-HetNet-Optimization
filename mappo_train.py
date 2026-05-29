# ============================================================
# RESEARCH-GRADE MAPPO TRAINER FOR 5G HETNET
# ============================================================
import sys
import os
import time
import random
import subprocess
import numpy as np
import pandas as pd

import torch
import torch.nn as nn
import torch.optim as optim

import csv


from torch.distributions import Categorical

# ============================================================
# DEVICE
# ============================================================

DEVICE = torch.device(
    "cuda" if torch.cuda.is_available()
    else "cpu"
)

print("Using Device:", DEVICE)

# ============================================================
# HYPERPARAMETERS
# ============================================================

NUM_AGENTS = 6

STATE_DIM_PER_AGENT = 4

GLOBAL_STATE_DIM = \
    NUM_AGENTS * STATE_DIM_PER_AGENT

ACTION_DIM = 8

LR = 3e-4

GAMMA = 0.99

EPS_CLIP = 0.2

K_EPOCHS = 10

MAX_EPISODES = 200

UPDATE_TIMESTEP = 5

ENTROPY_COEF = 0.01

VALUE_COEF = 0.5

MAX_GRAD_NORM = 0.5

# ============================================================
# CREATE DIRECTORIES
# ============================================================

os.makedirs(
    "models",
    exist_ok=True
)

os.makedirs(
    "checkpoints",
    exist_ok=True
)

os.makedirs(
    "logs",
    exist_ok=True
)

# ============================================================
# MEMORY BUFFER
# ============================================================

class Memory:

    def __init__(self):

        self.states = []

        self.actions = []

        self.logprobs = []

        self.rewards = []

        self.is_terminals = []

    def clear(self):

        del self.states[:]

        del self.actions[:]

        del self.logprobs[:]

        del self.rewards[:]

        del self.is_terminals[:]

# ============================================================
# ACTOR NETWORK
# ============================================================

class ActorNetwork(nn.Module):

    def __init__(self):

        super(ActorNetwork, self).__init__()

        self.network = nn.Sequential(

            nn.Linear(
                STATE_DIM_PER_AGENT,
                128
            ),

            nn.ReLU(),

            nn.Linear(
                128,
                128
            ),

            nn.ReLU(),

            nn.Linear(
                128,
                ACTION_DIM
            ),

            nn.Softmax(dim=-1)
        )

    def forward(self, x):

        return self.network(x)

# ============================================================
# CENTRALIZED CRITIC
# ============================================================

class CriticNetwork(nn.Module):

    def __init__(self):

        super(CriticNetwork, self).__init__()

        self.network = nn.Sequential(

            nn.Linear(
                GLOBAL_STATE_DIM,
                256
            ),

            nn.ReLU(),

            nn.Linear(
                256,
                256
            ),

            nn.ReLU(),

            nn.Linear(
                256,
                1
            )
        )

    def forward(self, x):

        return self.network(x)

# ============================================================
# MAPPO AGENT
# ============================================================

class MAPPO:

    def __init__(self):

        self.actors = [

            ActorNetwork().to(DEVICE)

            for _ in range(NUM_AGENTS)
        ]

        self.old_actors = [

            ActorNetwork().to(DEVICE)

            for _ in range(NUM_AGENTS)
        ]

        for i in range(NUM_AGENTS):

            self.old_actors[i].load_state_dict(
                self.actors[i].state_dict()
            )

        self.critic = CriticNetwork().to(DEVICE)

        self.actor_optimizers = [

            optim.Adam(
                actor.parameters(),
                lr=LR
            )

            for actor in self.actors
        ]

        self.critic_optimizer = optim.Adam(
            self.critic.parameters(),
            lr=LR
        )

        self.mse_loss = nn.MSELoss()

    # ========================================================
    # ACTION SELECTION
    # ========================================================

    def select_actions(
        self,
        states,
        memory
    ):

        actions = []

        logprobs = []

        for i in range(NUM_AGENTS):

            state = torch.FloatTensor(
                states[i]
            ).to(DEVICE)

            probs = self.old_actors[i](
                state
            )

            dist = Categorical(probs)

            action = dist.sample()

            actions.append(
                action.item()
            )

            logprobs.append(
                dist.log_prob(action)
            )

        memory.states.append(states)

        memory.actions.append(actions)

        memory.logprobs.append(logprobs)

        return actions
# ========================================================
# UPDATE POLICY
# ========================================================

def update(self, memory):

    # ====================================================
    # SAFETY CHECK
    # ====================================================

    if len(memory.rewards) < 2:

        print("WARNING: Not enough rewards for MAPPO update")

        return

    if len(memory.logprobs) == 0:

        print("WARNING: Empty logprobs")

        return

    rewards = []

    discounted_reward = 0

    for reward, is_terminal in zip(

        reversed(memory.rewards),

        reversed(memory.is_terminals)

    ):

        if is_terminal:

            discounted_reward = 0

        discounted_reward = (

            reward +

            (GAMMA * discounted_reward)

        )

        rewards.insert(
            0,
            discounted_reward
        )

    rewards = torch.tensor(

        rewards,

        dtype=torch.float32

    ).to(DEVICE)

    # ====================================================
    # SAFE NORMALIZATION
    # ====================================================

    if rewards.numel() > 1:

        rewards = (

            rewards - rewards.mean()

        ) / (

            rewards.std() + 1e-7
        )

        # ====================================================
        # TRAIN ACTORS
        # ====================================================

        for agent_id in range(NUM_AGENTS):

            old_states = []

            for s in memory.states:

                old_states.append(
                    s[agent_id]
                )

            old_states = torch.FloatTensor(
                old_states
            ).to(DEVICE)

            old_actions = torch.tensor(

                [a[agent_id]
                 for a in memory.actions]

            ).to(DEVICE)

            old_logprobs = torch.stack(

                [lp[agent_id]
                 for lp in memory.logprobs]

            ).detach().to(DEVICE)

            for _ in range(K_EPOCHS):

                probs = self.actors[agent_id](
                    old_states
                )

                dist = Categorical(probs)

                logprobs = dist.log_prob(
                    old_actions
                )

                entropy = dist.entropy()

                ratios = torch.exp(
                    logprobs - old_logprobs
                )

                advantages = rewards

                surr1 = \
                    ratios * advantages

                surr2 = torch.clamp(

                    ratios,

                    1 - EPS_CLIP,

                    1 + EPS_CLIP

                ) * advantages

                actor_loss = (

                    -torch.min(
                        surr1,
                        surr2
                    )

                    -

                    ENTROPY_COEF * entropy

                ).mean()

                self.actor_optimizers[
                    agent_id
                ].zero_grad()

                actor_loss.backward()

                torch.nn.utils.clip_grad_norm_(

                    self.actors[
                        agent_id
                    ].parameters(),

                    MAX_GRAD_NORM
                )

                self.actor_optimizers[
                    agent_id
                ].step()

        # ====================================================
        # TRAIN CENTRALIZED CRITIC
        # ====================================================

        global_states = []

        for s in memory.states:

            flat_state = []

            for agent_state in s:

                flat_state.extend(
                    agent_state
                )

            global_states.append(
                flat_state
            )

        global_states = torch.FloatTensor(
            global_states
        ).to(DEVICE)

        state_values = self.critic(
            global_states
        ).squeeze()

        critic_loss = self.mse_loss(
            state_values,
            rewards
        )

        self.critic_optimizer.zero_grad()

        critic_loss.backward()

        torch.nn.utils.clip_grad_norm_(

            self.critic.parameters(),

            MAX_GRAD_NORM
        )

        self.critic_optimizer.step()


        # ====================================================
        # UPDATE OLD POLICIES
        # ====================================================

        for i in range(NUM_AGENTS):

            self.old_actors[i].load_state_dict(

                self.actors[i].state_dict()

            )


# ============================================================
# GET STATE FROM NS3 CSV
# ============================================================

def get_state():

    try:

        df = pd.read_csv(
            "rl_state.csv"
        )

        row = df.iloc[-1]

        states = []

        for i in range(NUM_AGENTS):

            state = [

                row[f"SBS{i}_Load"],

                row[f"SBS{i}_SINR"],

                row[f"SBS{i}_PRB"],

                row[f"SBS{i}_Power"]

            ]


            states.append(state)

        # ====================================================
        # REWARD
        # ====================================================

        reward = row["Reward"]


        # ====================================================
        # NETWORK KPIs
        # ====================================================

        throughput = row["Throughput"]

        delay = row["Delay"]

        energy_efficiency = row["EnergyEfficiency"]

        packet_loss = row["PacketLoss"]

        sleep_ratio = row["SleepRatio"]

        # ====================================================
        # AVERAGE SINR
        # ====================================================

        sinr = (

            row["SBS0_SINR"] +

            row["SBS1_SINR"] +

            row["SBS2_SINR"] +

            row["SBS3_SINR"] +

            row["SBS4_SINR"] +

            row["SBS5_SINR"]

        ) / NUM_AGENTS
        return (

            states,

            reward,

            throughput,

            delay,

            sinr,

            energy_efficiency,

            packet_loss,

            sleep_ratio
        )

    except Exception as e:

        print(
            "State Processing Error:",
            e
        )

        dummy_state = [

            [0.0] * STATE_DIM_PER_AGENT

            for _ in range(NUM_AGENTS)
        ]

        return (
            dummy_state,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0
        )

# ============================================================
# WRITE MULTI-AGENT ACTIONS
# ============================================================

def write_actions(actions):

    with open(
        "action.txt",
        "w"
    ) as f:

        for a in actions:

            f.write(f"{a} ")

# ============================================================
# RUN NS3 SIMULATION
# ============================================================

def run_ns3():

    try:

        subprocess.run(

            [
                "./ns3",
                "run",
                "scratch/hetnet-mappo"
            ],

            check=True
        )

    except Exception as e:

        print("NS3 Error:", e)

# ============================================================
# MAIN TRAINING LOOP
# ============================================================

memory = Memory()

mappo = MAPPO()

best_reward = -1e9

# ============================================================
# CREATE CSV FILE
# ============================================================

with open("mappo_training_log.csv", "w") as f:

    f.write(
        "Episode,Reward,Throughput,Delay,SINR,EnergyEfficiency,PacketLoss,SleepRatio\n"
    )

print("===================================")
print("STARTING RESEARCH-GRADE MAPPO")
print("===================================")

timestep = 0

for episode in range(MAX_EPISODES):

    print("===================================")
    print("Episode:", episode)
    sys.stdout.flush()
    print("===================================")

    # GET STATE
    states, _, _, _, _, _, _, _ = get_state()

    # SELECT ACTIONS
    actions = mappo.select_actions(
        states,
        memory
    )

    print("Selected Actions:", actions)

    # WRITE ACTIONS
    write_actions(actions)

    # RUN NS3
    run_ns3()

    # GET NEXT STATE
    next_states, reward, throughput, delay, sinr, energy_efficiency, packet_loss, sleep_ratio = get_state()

    print("Reward:", reward)
    sys.stdout.flush()
    # SAVE CSV
    with open("mappo_training_log.csv", "a") as f:

        f.write(
            f"{episode},"
            f"{reward},"
            f"{throughput},"
            f"{delay},"
            f"{sinr},"
            f"{energy_efficiency},"
            f"{packet_loss},"
            f"{sleep_ratio}\n"
        )

    done = reward < -80

    memory.rewards.append(reward)

    memory.is_terminals.append(done)

    timestep += 1
# ========================================================
# UPDATE MAPPO
# ========================================================

if timestep % UPDATE_TIMESTEP == 100:

    print("Updating MAPPO Policy...")

    mappo.update(memory)

    memory.clear()

    # ========================================================
    # UPDATE MAPPO
    # ========================================================

    if timestep % UPDATE_TIMESTEP == 0:

        print("Updating MAPPO Policy...")

        mappo.update(memory)

        memory.clear()

    # ========================================================
    # SAVE BEST MODEL
    # ========================================================

    if reward > best_reward:

        best_reward = reward

        for i in range(NUM_AGENTS):

            torch.save(

                mappo.actors[i].state_dict(),

                f"models/mappo_actor_{i}.pth"

            )

        torch.save(

            mappo.critic.state_dict(),

            "models/mappo_critic.pth"
        )

        print("Best MAPPO Model Saved")

    # ========================================================
    # SAVE CHECKPOINTS
    # ========================================================

    if episode % 25 == 0:

        for i in range(NUM_AGENTS):

            torch.save(

                mappo.actors[i].state_dict(),

                f"checkpoints/mappo_actor_{i}_{episode}.pth"

            )

        print("Checkpoint Saved")

print("===================================")
print("MAPPO TRAINING COMPLETED")
