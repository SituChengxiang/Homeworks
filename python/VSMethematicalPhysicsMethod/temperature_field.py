import numpy as np
import matplotlib.pyplot as plt
from scipy.integrate import quad
from scipy.special import j0
from mpl_toolkits.mplot3d import Axes3D
import os
import sys

# ==========================================
# 1. 参数定义 (Parameters)
# ==========================================
h0 = 0.5e-3      # 液体厚度 (m)
R = 2.0e-3       # 加热半径 (m)
T_max = 100.0     # 温度参数 (K)
Bi = 0.0321      # 毕奥数 (Dimensionless)

# 奇异点位置 (Singularity location)
LAMBDA_SING = Bi / h0

print(f"Parameters:")
print(f"  h0 = {h0*1000} mm")
print(f"  R  = {R*1000} mm")
print(f"  Bi = {Bi}")
print(f"  Singularity at lambda = {LAMBDA_SING:.4f}")

# ==========================================
# 2. 积分函数 (Integration Functions)
# ==========================================

def integrand(lam, r, z):
    """
    被积函数 T(r,z) 的核心部分
    """
    # 衰减项
    decay = np.exp(- (lam * R)**2 / 4)
    
    # z 方向项
    numerator = h0 * lam + Bi
    denominator = h0 * lam - Bi
    
    # 避免除以零 (虽然积分区间会避开，但为了安全)
    if abs(denominator) < 1e-15:
        return 0
        
    term_z = np.exp(lam * z) + (numerator / denominator) * np.exp(lam * (2 * h0 - z))
    
    # 贝塞尔函数项
    bessel = j0(lam * r)
    
    return decay * term_z * bessel * lam

def calculate_temperature_point(r, z):
    """
    计算单点 (r, z) 的温度 T(r, z)
    使用 Cauchy Principal Value (CPV) 方法处理奇点
    """
    coeff = (T_max * R**2) / 2
    
    # 设定积分上下限和避开奇点的 epsilon
    eps = 1e-5
    
    # 积分区间分为两部分，跳过奇点
    # 1. [0, LAMBDA_SING - eps]
    val1, err1 = quad(integrand, 0, LAMBDA_SING - eps, args=(r, z), limit=1000)
    
    # 2. [LAMBDA_SING + eps, inf]
    # 上限设为足够大即可，因为高斯衰减很快
    limit_inf = 5000 
    val2, err2 = quad(integrand, LAMBDA_SING + eps, limit_inf, args=(r, z), limit=1000)
    
    integral_val = val1 + val2
    
    return coeff * integral_val

def calculate_temperature_field(r_array, z_array):
    """
    计算网格上的温度场
    """
    T = np.zeros((len(z_array), len(r_array)))
    
    print("Calculating temperature field...")
    total_points = len(r_array) * len(z_array)
    count = 0
    
    for i, z in enumerate(z_array):
        for j, r in enumerate(r_array):
            val = calculate_temperature_point(r, z)
            T[i, j] = val
            count += 1
            if count % 50 == 0:
                print(f"  Progress: {count}/{total_points} points calculated. Last val: {val:.4f}", end='\r')
    
    print(f"\nCalculation complete.")
    return T

# ==========================================
# 3. 主程序与绘图 (Main & Plotting)
# ==========================================

def main():
    # --------------------------------------
    # A. 计算数据
    # --------------------------------------
    # 定义网格
    r_vals = np.linspace(0, 10e-3, 50)  # 50 points
    z_vals = np.linspace(0, h0, 20)     # 20 points
    
    T_field = calculate_temperature_field(r_vals, z_vals)
    
    # Debug: Check data stats
    print("\nData Statistics:")
    print(f"  Min: {np.min(T_field)}")
    print(f"  Max: {np.max(T_field)}")
    print(f"  Mean: {np.mean(T_field)}")
    print(f"  NaN count: {np.isnan(T_field).sum()}")
    
    if np.isnan(T_field).all():
        print("Error: All values are NaN!")
        return

    # 保存数据
    np.save('data.npy', T_field)
    print("Data saved to 'data.npy'")

    # --------------------------------------
    # B. 提取关键数据 (表面温度梯度)
    # --------------------------------------
    T_surface = T_field[-1, :]
    dr = r_vals[1] - r_vals[0]
    dT_dr = np.gradient(T_surface, dr)
    
    abs_dT_dr = np.abs(dT_dr)
    max_grad_idx = np.argmax(abs_dT_dr)
    max_grad_val = abs_dT_dr[max_grad_idx]
    r_max_grad = r_vals[max_grad_idx]
    
    summary = f"# Surface |dT/dr|_max = {max_grad_val:.2f} K/m at r = {r_max_grad*1000:.2f} mm"
    print("-" * 60)
    print(summary)
    print("-" * 60)

    # --------------------------------------
    # C. 绘图
    # --------------------------------------
    plt.style.use('default')
    
    # 1. 表面温度曲线
    try:
        plt.figure(figsize=(8, 6))
        plt.plot(r_vals * 1000, T_surface, 'b-', linewidth=2, label=f'z = {h0*1000} mm')
        plt.xlabel('Radius r (mm)')
        plt.ylabel('Temperature T (K)')
        plt.title('Surface Temperature Distribution')
        plt.grid(True, linestyle='--', alpha=0.7)
        plt.legend()
        plt.savefig('surface_temperature.png', dpi=300)
        print("Saved 'surface_temperature.png'")
    except Exception as e:
        print(f"Error plotting surface temperature: {e}")

    # 2. 温度场云图
    try:
        R_grid, Z_grid = np.meshgrid(r_vals * 1000, z_vals * 1000)
        plt.figure(figsize=(10, 6))
        # 使用 contourf 时，如果数据范围极小或全为0，可能会出问题
        # 添加 levels 确保能画出来
        if np.max(T_field) - np.min(T_field) < 1e-9:
             print("Warning: Data is constant or near constant.")
             
        cp = plt.contourf(R_grid, Z_grid, T_field, levels=20, cmap='RdYlBu_r')
        cbar = plt.colorbar(cp)
        cbar.set_label('Temperature (K)')
        plt.xlabel('Radius r (mm)')
        plt.ylabel('Height z (mm)')
        plt.title('Temperature Field Contour (r-z plane)')
        plt.savefig('temperature_contour.png', dpi=300)
        print("Saved 'temperature_contour.png'")
    except Exception as e:
        print(f"Error plotting contour: {e}")

    # 3. 3D 曲面图
    try:
        fig = plt.figure(figsize=(12, 8))
        ax = fig.add_subplot(111, projection='3d')
        surf = ax.plot_surface(R_grid, Z_grid, T_field, cmap='RdYlBu_r', edgecolor='none', alpha=0.9)
        fig.colorbar(surf, ax=ax, shrink=0.5, aspect=10, label='Temperature (K)')
        ax.set_xlabel('Radius r (mm)')
        ax.set_ylabel('Height z (mm)')
        ax.set_zlabel('Temperature T (K)')
        ax.set_title('3D Temperature Field')
        ax.view_init(elev=30, azim=45)
        plt.savefig('temperature_3d.png', dpi=300)
        print("Saved 'temperature_3d.png'")
    except Exception as e:
        print(f"Error plotting 3D: {e}")

    # 4. 表面温度分布图 (俯视图/圆盘图)
    try:
        # 创建极坐标网格
        theta = np.linspace(0, 2*np.pi, 100)
        # r_vals 是 1D 数组，需要扩展
        r_grid_disk, theta_grid = np.meshgrid(r_vals * 1000, theta)
        
        # T_surface 是 1D 数组 (对应 r)，需要沿 theta 方向复制
        # T_surface shape: (50,), T_disk shape: (100, 50)
        T_disk = np.tile(T_surface, (len(theta), 1))
        
        fig = plt.figure(figsize=(10, 8))
        ax = fig.add_subplot(111, projection='polar')
        
        # 绘制热力图
        c = ax.pcolormesh(theta_grid, r_grid_disk, T_disk, cmap='RdYlBu_r', shading='auto')
        
        cbar = fig.colorbar(c, ax=ax, label='Temperature (K)', shrink=0.8)
        ax.set_title('Surface Temperature Distribution (Top View)', pad=20)
        
        # 调整网格和标签
        ax.grid(True, alpha=0.3)
        ax.set_yticks(np.linspace(0, 10, 6))  # 0, 2, 4, 6, 8, 10 mm
        ax.set_yticklabels([f'{x:.0f}mm' for x in np.linspace(0, 10, 6)])
        
        plt.savefig('surface_temperature_disk.png', dpi=300)
        print("Saved 'surface_temperature_disk.png'")
    except Exception as e:
        print(f"Error plotting disk view: {e}")
    
    plt.close('all')

if __name__ == "__main__":
    main()
