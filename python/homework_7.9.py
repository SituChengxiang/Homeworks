#学号:
#姓名:
#运行作业题的程序时将相应题目中语句前的#号删掉
from fileinput import close

#=================================作业题1========================================
#已知学生信息如下：
student = {
    "id": "2024001",
    "name": "小明",
    "age": 19,
    "scores": [85, 92, 78],
    "is_active": True
}

"""(1). 将学生数据保存到 student.json 文件中,要求使用 with open() 打开文件,
使用 json.dump() 写入数据。 """

import json
#在下面写自己的程序
with open("student.json","w") as fp:
    json.dump(student, fp)

#在上面写自己的程序


"""(2). 读取 JSON 文件，要求使用 with open() 打开文件，使用 json.load() 读取数据，
打印读取到的数据，计算学生的平均分并显示。"""

#在下面写自己的程序
with open("student.json","r") as fp:
    data = json.load(fp)
    print(data)
    sum = 0
    scores = list(data["scores"])
    for score in scores:
        sum += score
    average = sum / len(scores)
    print(int(average))

#在上面写自己的程序


#=================================作业题2========================================
#给定列表：
a = [("张三", 18, 95.5), ("李四", 19,True),{1, 2, 3, 4, 5},{"apple", "banana", "cherry"},
     ["列表元素", 1, 2, 3],{"姓名": "王五","年龄": 20},12345,3.14159,"字符串元素"]

"""使用pickle模块将将列表a保存到二进制文件lista.dat文件中,并重新load到列表b中，
打印b中类型为元组和集合的元素。"""

import pickle
#在下面写自己的程序
with open("lista.dat","wb") as fp:
    pickle.dump(a,fp)

with open("lista.dat","rb") as fp:
    b = pickle.load(fp)
    for item in b:
        if isinstance(item,tuple):
            print(item)
        elif isinstance(item,set):
            print(item)

#在上面写自己的程序








