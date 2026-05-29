# ============================================================
# RESEARCH-GRADE PPO FOR 5G HETNET
# PPO + GAE + LSTM + STABLE TRAINING
# ============================================================

import os
import time
import random
import subprocess
import numpy as np
import pandas as pd

import torch
import torch.nn as nn
import torch.optim as optim

from torch.distributions import Categorical

# ============================================================
# DEVICE
# ============================================================

DEVICE = torch.device(
    "cuda" if torch.cuda.is_available() else "cpu"
)

print("Using Device:", DEVICE)

# ============================================================
# HYPERPARAMETERS
# ============================================================

STATE_DIM = 12

ACTION_DIM = 8

LR = 3e-4

GAMMA = 0.99

GAE_LAMBDA = 0.95

EPS_CLIP = 0.2

K_EPOCHS = 10

MAX_EPISODES = 200

UPDATE_TIMESTEP = 5

# ============================================================
# PPO LOSS COEFFICIENTS
# ============================================================

ENTROPY_COEF = 0.02

VALUE_COEF = 0.5

MAX_GRAD_NORM = 0.5

# ============================================================
# Exploration Parameter
# ============================================================

EPSILON_START = 0.15

EPSILON_DECAY = 0.995

EPSILON_MIN = 0.05
# ============================================================
# TRAINING CSV LOG
# ============================================================

if not os.path.exists("ppo_training_log.csv"):

    with open(
        "ppo_training_log.csv",
        "w"
    ) as f:

        f.write(
            "Episode,Reward,Action,Throughput,Delay,SINR,EnergyEfficiency,SleepRatio\n"
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
        self.state_values = []

    def clear(self):

        self.states.clear()
        self.actions.clear()
        self.logprobs.clear()
        self.rewards.clear()
        self.is_terminals.clear()
        self.state_values.clear()

# ============================================================
# GAE ADVANTAGE
# ============================================================

def compute_gae(
    rewards,
    values,
    dones,
    gamma=0.99,
    lam=0.95
):

    advantages = []

    gae = 0
    next_value = 0

    for step in reversed(range(len(rewards))):

        delta = (
            rewards[step]
            +
            gamma * next_value * (1 - dones[step])
            -
            values[step]
        )

        gae = (
            delta
            +
            gamma * lam
            * (1 - dones[step])
            * gae
        )

        advantages.insert(0, gae)

        next_value = values[step]

    return advantages

# ============================================================
# ACTOR-CRITIC NETWORK
# ============================================================

class ActorCritic(nn.Module):

    def __init__(self):

        super(ActorCritic, self).__init__()

        # ====================================================
        # LSTM FEATURE EXTRACTOR
        # ====================================================

        self.lstm = nn.LSTM(
            input_size=STATE_DIM,
            hidden_size=128,
            num_layers=1,
            batch_first=True
        )

        # ====================================================
        # ACTOR NETWORK
        # ====================================================

        self.actor = nn.Sequential(

            nn.Linear(128, 128),
            nn.ReLU(),

            nn.Linear(128, ACTION_DIM),
            nn.Softmax(dim=-1)
        )

        # ====================================================
        # CRITIC NETWORK
        # ====================================================

        self.critic = nn.Sequential(

            nn.Linear(128, 128),
            nn.ReLU(),

            nn.Linear(128, 1)
        )

    def forward(self, x):

        if len(x.shape) == 2:

            x = x.unsqueeze(1)

        lstm_out, _ = self.lstm(x)

        features = lstm_out[:, -1, :]

        probs = self.actor(features)

        probs = torch.clamp(
            probs,
            1e-8,
            1.0
        )

        probs = probs / probs.sum(
            dim=-1,
            keepdim=True
        )

        value = self.critic(features)

        return probs, value

# ============================================================
# PPO AGENT
# ============================================================

class PPO:

    def __init__(self):

        self.policy = ActorCritic().to(DEVICE)

        self.policy_old = ActorCritic().to(DEVICE)

        self.policy_old.load_state_dict(
            self.policy.state_dict()
        )

        self.optimizer = optim.Adam(
            self.policy.parameters(),
            lr=LR
        )

        self.mse_loss = nn.MSELoss()

    # ========================================================
    # ACTION SELECTION
    # ========================================================

    def select_action(
        self,
        state,
        memory,
        episode
    ):

        state_tensor = torch.FloatTensor(
            state
        ).unsqueeze(0).to(DEVICE)

        with torch.no_grad():

            probs, state_value = \
                self.policy_old(state_tensor)

        probs = torch.clamp(
            probs,
            1e-8,
            1.0
        )

        probs = probs / probs.sum(
            dim=-1,
            keepdim=True
        )

        dist = Categorical(probs)

        # ====================================================
        # EPSILON EXPLORATION
        # ====================================================

        epsilon = max(
             EPSILON_MIN,
             EPSILON_START *
             (0.997 ** episode)
        )
        if random.random() < epsilon:

            action = random.randint(
                0,
                ACTION_DIM - 1
            )

        else:

            action = dist.sample().item()

        action_tensor = torch.tensor(
            action
        ).to(DEVICE)

        logprob = dist.log_prob(
            action_tensor
        )

        memory.states.append(
            state_tensor.cpu()
        )

        memory.actions.append(action)

        memory.logprobs.append(
            logprob.cpu()
        )

        memory.state_values.append(
            state_value.item()
        )

        return action

    # ========================================================
    # PPO UPDATE
    # ========================================================

    def update(self, memory):

        rewards = []

        discounted_reward = 0

        for reward, is_terminal in zip(
            reversed(memory.rewards),
            reversed(memory.is_terminals)
        ):

            if is_terminal:

                discounted_reward = 0

            discounted_reward = (
                reward
                +
                GAMMA * discounted_reward
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
        # REWARD NORMALIZATION
        # ====================================================

        if len(rewards) > 1:

            rewards = (
                rewards - rewards.mean()
            ) / (
                rewards.std() + 1e-7
            )

        # ====================================================
        # GAE ADVANTAGES
        # ====================================================

        advantages = compute_gae(
            memory.rewards,
            memory.state_values,
            memory.is_terminals,
            GAMMA,
            GAE_LAMBDA
        )

        advantages = torch.tensor(
            advantages,
            dtype=torch.float32
        ).to(DEVICE)
        advantages = (
            advantages - advantages.mean()
        ) / (
            advantages.std() + 1e-8
        )


        old_states = torch.cat(
            list(memory.states)
        ).detach().to(DEVICE)

        old_actions = torch.tensor(
            list(memory.actions)
        ).detach().to(DEVICE)

        old_logprobs = torch.stack(
            list(memory.logprobs)
        ).detach().to(DEVICE)

        # ====================================================
        # PPO TRAINING LOOP
        # ====================================================

        for _ in range(K_EPOCHS):

            probs, state_values = \
                self.policy(old_states)

            probs = torch.clamp(
                probs,
                1e-8,
                1.0
            )

            probs = probs / probs.sum(
                dim=-1,
                keepdim=True
            )

            dist = Categorical(probs)

            logprobs = dist.log_prob(
                old_actions
            )

            dist_entropy = \
                dist.entropy().mean()

            ratios = torch.exp(
                logprobs - old_logprobs
            )

            surr1 = ratios * advantages

            surr2 = torch.clamp(
                ratios,
                1 - EPS_CLIP,
                1 + EPS_CLIP
            ) * advantages

            # ================================================
            # ACTOR LOSS
            # ================================================

            actor_loss = -torch.min(
                surr1,
                surr2
            ).mean()

            # ================================================
            # CRITIC LOSS
            # ================================================

            critic_loss = self.mse_loss(
                state_values.view(-1),
                rewards
            )

            # ================================================
            # TOTAL LOSS
            # ================================================

            loss = (
                actor_loss
                +
                VALUE_COEF * critic_loss
                -
                ENTROPY_COEF * dist_entropy
            )

            self.optimizer.zero_grad()

            loss.backward()
            print(
                f"Actor Loss: {actor_loss.item():.4f} | "
                f"Critic Loss: {critic_loss.item():.4f}"
           )

            # ================================================
            # GRADIENT CLIPPING
            # ================================================

            torch.nn.utils.clip_grad_norm_(
                self.policy.parameters(),
                MAX_GRAD_NORM
            )

            self.optimizer.step()

        # ====================================================
        # POLICY UPDATE
        # ====================================================

        self.policy_old.load_state_dict(
            self.policy.state_dict()
        )

# ============================================================
# RL STATE EXTRACTION
# ============================================================

def get_state():

    if not os.path.exists("rl_state.csv"):

        print("CSV file not found")

        return np.zeros(STATE_DIM), 0.0

    time.sleep(1)

    try:

        df = pd.read_csv(
            "rl_state.csv",
            on_bad_lines='skip'
        )

    except Exception as e:

        print("CSV Read Error:", e)

        return np.zeros(STATE_DIM), 0.0

    if len(df) == 0:

        print("CSV Empty")

        return np.zeros(STATE_DIM), 0.0

    df = df.dropna()

    if len(df) == 0:

        print("CSV Contains NaN")

        return np.zeros(STATE_DIM), 0.0

    row = df.iloc[-1]

    try:

        state = np.array([

            row["Throughput"] / 100.0,
            row["Delay"] / 0.1,
            row["EnergyEfficiency"] / 5.0,
            row["TotalPower"] / 100.0,
            row["SINR"] / 35.0,
            row["ActiveSBS"] / 6.0,
            row["PacketLoss"] / 100.0,
            row["BackhaulUtilization"] / 100.0,
            row["SleepRatio"] / 1.0,
            row["SBS0_Load"] / 100.0,
            row["SBS0_PRB"] / 100.0,
            row["SBS0_SINR"] / 35.0

        ], dtype=np.float32)

        reward = float(row["Reward"])

        reward = np.clip(
            reward,
            -80,
            80
        )

        return state, reward

    except Exception as e:

        print("State Processing Error:", e)

        return np.zeros(STATE_DIM), 0.0


# ============================================================
# MAIN TRAINING LOOP
# ============================================================

memory = Memory()

ppo = PPO()

best_reward = -999999

# ============================================================
# LOAD EXISTING PPO MODEL
# ============================================================

if os.path.exists("models/ppo_best.pth"):

    ppo.policy.load_state_dict(
        torch.load(
            "models/ppo_best.pth",
            map_location=DEVICE
        )
    )

    ppo.policy_old.load_state_dict(
        ppo.policy.state_dict()
    )

    print("Loaded Existing PPO Model")

print("===================================")
print("STARTING RESEARCH-GRADE PPO")
print("===================================")

timestep = 0

# ============================================================
# TRAINING LOOP
# ============================================================

for episode in range(MAX_EPISODES):

    print("===================================")
    print("Episode:", episode)
    print("===================================")

    # ========================================================
    # GET CURRENT STATE
    # ========================================================

    state, _ = get_state()

    # ========================================================
    # SELECT ACTION
    # ========================================================

    action = ppo.select_action(
        state,
        memory,
        episode
    )

    print("Selected Action:", action)

    # ========================================================
    # EXPORT ACTION TO NS3
    # ========================================================

    with open("action.txt", "w") as f:

        f.write(str(action))

    # ========================================================
    # RUN NS3 SIMULATION
    # ========================================================

    subprocess.run([
        "./ns3",
        "run",
        "scratch/hetnet-mappo"
    ])

    # ========================================================
    # GET NEXT STATE + REWARD
    # ========================================================

    next_state, reward = get_state()

    done = (
        episode == MAX_EPISODES - 1
        
        or
        reward < -60
    )

    memory.rewards.append(reward)

    memory.is_terminals.append(done)

    timestep += 1

    print("Reward:", reward)
    print(
        f"Throughput: {next_state[0]*100:.2f} Mbps | "
        f"SINR: {next_state[4]*35:.2f} dB | "
        f"EE: {next_state[2]*5:.2f}"
   )

# ========================================================
# SAVE PPO TRAINING LOG
# ========================================================

with open(
    "ppo_training_log.csv",
    "a"
) as f:

    f.write(

        f"{episode},"

        f"{reward},"

        f"{action},"

        f"{next_state[0]},"

        f"{next_state[1]},"

        f"{next_state[4]},"

        f"{next_state[2]},"

        f"{next_state[11]}\n"
    )
# ========================================================
# PPO UPDATE
# ========================================================

if timestep % UPDATE_TIMESTEP == 0:

    print("Updating PPO Policy...")

    ppo.update(memory)

    memory.clear()

# ========================================================
# SAVE BEST MODEL
# ========================================================

if reward > best_reward:

    best_reward = reward

    torch.save(
        ppo.policy.state_dict(),
        "models/ppo_best.pth"
    )

    print("Best Model Saved")

# ========================================================
# PERIODIC CHECKPOINT SAVE
# ========================================================

if episode % 25 == 0:

    torch.save(
        ppo.policy.state_dict(),
        f"checkpoints/checkpoint_{episode}.pth"
    )

    print("Checkpoint Saved")

print("===================================")
print("TRAINING COMPLETED")
print("===================================")
