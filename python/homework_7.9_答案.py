#学号:
#姓名:
#运行作业题的程序时将相应题目中语句前的#号删掉


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
with open('student.json', 'w') as fp:
        json.dump(student, fp)
#在上面写自己的程序


"""(2). 读取 JSON 文件，要求使用 with open() 打开文件，使用 json.load() 读取数据，
打印读取到的数据，计算学生的平均分并显示。"""

#在下面写自己的程序
with open('student.json', 'r') as fp:
        loaded_student = json.load(fp)

# 逐项显示学生信息
print(f"学号: {loaded_student['id']}")
print(f"姓名: {loaded_student['name']}")
print(f"年龄: {loaded_student['age']}")
print(f"成绩列表: {loaded_student['scores']}")
print(f"是否在校: {'是' if loaded_student['is_active'] else '否'}")

 # 计算学生的平均分
scores = loaded_student['scores']  # 获取成绩列表
total_score = sum(scores)          # 计算总分
count = len(scores)                # 计算科目数量
average_score = total_score / count  # 计算平均分
print(f"  平均分: {average_score:.2f} 分")  # 保留两位小数

#在上面写自己的程序


#=================================作业题2========================================
#给定列表：
a = [("张三", 18, 95.5), ("李四", 19,True),{1, 2, 3, 4, 5},{"apple", "banana", "cherry"},
     ["列表元素", 1, 2, 3],{"姓名": "王五","年龄": 20},12345,3.14159,"字符串元素"]

"""使用pickle模块将将列表a保存到二进制文件lista.dat文件中,并重新load到列表b中，
打印b中类型为元组和集合的元素。"""

import pickle
#在下面写自己的程序
with open('lista.dat', 'wb') as fp:
    pickle.dump(a, fp)
with open('lista.dat', 'rb') as fp:
    b = pickle.load(fp)
for v in b:
    if type(v) in (tuple, set):
        print(v)
#在上面写自己的程序








