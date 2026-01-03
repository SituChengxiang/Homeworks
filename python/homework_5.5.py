#学号: 
#姓名: 
#运行作业题的程序时将相应题目中语句前的#号删掉



#=================================作业题1========================================
"""1. 写一个递归函数print_int(a)，以嵌套列表a为参数，打印a及其所有子列表中的整数。
并以实参[[1,2,(3.0,4)], (5.0,6), 7]调用它，写下程序和输出结果。"""


#在下面写自己的程序
def print_int(a):
    if isinstance(a, (list, tuple,set)):
        for item in a:
            print_int(item)
    elif isinstance(a, int):
        print(a)
    # 忽略 float、str 等其他类型（题目中只有 int/float/list/tuple）

# 调用
print_int([[1,2,(3.0,4)], (5.0,6), 7])
#在上面写自己的程序
# output:
# 1
# 2
# 4
# 6
# 7

#=================================作业题2========================================
"""(1) 试写一个修饰器函数our_decorator(func)，被修饰的函数有两个参数。
要求被修饰的函数依次打印各个位置参数的值和函数func的返回值，请都打印在一行里面。"""

#在下面写自己的程序
def our_decorator(func):
    def wrapper(*args,**kwargs):
        a,b=args[0],args[1]
        result = func(a,b)
        print(a,b,result)
        return result
    return wrapper

#在上面写自己的程序


"""(2)使用our_decorator修饰 
def f(n,i):
    if n==i or i==0:
        return 1
    return f(n-1, i) + f(n-1, i-1)
并调用修饰后的函数：f(8, 3)"""

#在下面写自己的程序
@our_decorator
def f(n,i):
    if n==i or i==0:
        return 1
    return f(n-1, i) + f(n-1, i-1)

f(8,3)
#在上面写自己的程序
