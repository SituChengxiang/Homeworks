import numpy as np

# 模拟 state_means 为 3 个轴（x, y, z）
N = 3  # 时间点的数量
state_means = []
for axis in range(3):
    # 假设每个轴在每个时间点的位置、速度和加速度值相同
    means = np.array([
        [1, 2, 3],  # 时间点 0: 位置=1, 速度=2, 加速度=3
        [4, 5, 6],  # 时间点 1: 位置=4, 速度=5, 加速度=6
        [7, 8, 9]   # 时间点 2: 位置=7, 速度=8, 加速度=9
    ])
    state_means.append(means)

# 沿着第三个轴堆叠 means
means = np.stack(state_means, axis=2)
pos_std = means[:, 0, :]  # X/Y/Z 的位置 → 位置
vel_std = means[:, 1, :]  # X/Y/Z 的速度 → 速度
acc_std = means[:, 2, :]  # X/Y/Z 的加速度 → 加速度

print(means.shape)  # 应该是 (3, 3, 3)
print("所有时间点和所有轴的位置：")
print(means[:, 0, :])  # 所有时间点和所有轴的位置
print("所有时间点和所有轴的速度：")
print(means[:, 1, :])  # 所有时间点和所有轴的速度
print("所有时间点和所有轴的加速度：")
print(means[:, 2, :])  # 所有时间点和所有轴的加速度

print("pos_std 的内容:")
print(pos_std)
print("vel_std 的内容:")
print(vel_std)
print("acc_std 的内容:")
print(acc_std)