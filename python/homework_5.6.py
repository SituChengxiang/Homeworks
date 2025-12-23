#学号:
#姓名:
#运行作业题的程序时将相应题目中语句前的#号删掉

#=================================作业题1========================================
"""设计一个学生类Student,包括：
(1). 作为属性的姓名name、年龄age和成绩score.
(2). 实现实例方法isPassed方法，当score大于等于60分则返回True，否则返回False。
(3). 生成实例对象姓名：小明，年龄19随，成绩80分，并调用isPassed方法进行score判断。
"""

#在下面写自己的程序(注意:前面空四格)       
class Student:
    def __init__(self, name, age, score):
        self._name = name
        self._age = age
        self._score = score

    def isPassed(self):
        return self._score >= 60  

xiao_ming = Student('小明', 19, 80)
print(xiao_ming.isPassed())
 
#在上面写自己的程序 


#=================================作业题2========================================
"""设时间类Time的部分定义如下：在方法__init__内定义了三个数据成员_h、_m和_s表示
时间的时、分和秒。"""

"""1. 为类Time写一个类方法from_str，以字符串s为参数。当s是类似'10:5:20'的字符串时，
返回相应的对象Time(10, 5, 20)。 """

"""2. 为类Time写一个实例方法is_time，若Time对象的分和秒在0到59之间，则返回True，
否则返回False。"""


class Time:
    def __init__(self, h, m, s):
        self._h = h
        self._m = m
        self._s = s
        
#在下面写自己的程序(注意:前面空四格)       
    @classmethod
    def from_str(cls, s):
        h, m, s = map(int, s.split(':'))
        return Time(h, m, s)

    def is_time(self):
        return (0 <= self._m <= 59 and 0 <= self._s <= 59)
 
#在上面写自己的程序  
  
d1 = Time.from_str('10:5:20')
print(d1.is_time())
d2 = Time.from_str('10:80:20')
print(d2.is_time())








