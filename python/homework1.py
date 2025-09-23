#学号:
#姓名:
#运行作业题的程序时将相应题目的三引号删掉，交作业时保证删掉了所有三引号

#=================================作业题1========================================
#使用pip install安装requests包，然后写下pip list完整的输出
#请结果写在下面，每一行以#开头（！老师我用的 Conda，之前配置的环境里已经包含了一些预装的包了！）
#Package            Version
#------------------ -----------
# certifi            2025.8.3
# charset-normalizer 3.4.3
# contourpy          1.3.2
# cycler             0.12.1
# fonttools          4.59.0
# idna               3.10
# kiwisolver         1.4.8
# laspy              2.4.1
# matplotlib         3.10.3
# numpy              2.3.2
# pillow             11.3.0
# pip                25.2
# pybullet           3.2.7
# pyparsing          3.2.3
# python-dateutil    2.9.0.post0
# requests           2.32.5
# scipy              1.16.0
# setuptools         80.9.0
# six                1.17.0
# urllib3            2.5.0
# wheel              0.45.1


#=================================作业题2========================================
#设三角形三条边长分别为a,b,c，计算边长c所对的角(theta)的度数(单位为度，不是弧度)
#**表示幂运算，其他算术运算符同C语言


from random import random, uniform
from math import *

a = random()
b = random()
c = uniform(abs(a-b), a + b)
theta = None

#以变量theta表示角度
#在下面写自己的程序
cos_theta = (a**2 + b**2 - c**2) / (2 * a * b)
theta = degrees(acos(cos_theta))
#在上面写自己的程序

print(a, b, c, theta)



