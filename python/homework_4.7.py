#学号: 
#姓名: 
#运行作业题的程序时将相应题目中语句前的#号删掉


#=================================作业题1========================================

s = """STRAY birds of summer come to my window
to sing and fly away. 
And yellow leaves of autumn, 
which have no songs, 
flutter and fall there with a sigh. 

O TROUPE of little vagrants of the world, 
leave your footprints in my words.  

THE world puts off its mask of vastness to its lover. 
It becomes small as one song, 
as one kiss of the eternal. 

IT is the tears of the earth
that keep her smiles in bloom. 

THE mighty desert is burning 
for the love of a blade of grass 
who shakes her head and laughs 
and flies
away. 

IF you shed tears when you miss the sun,
you also miss the stars. 

THE sands in your way beg for your song 
and your movement, 
dancing water. 
Will you carry the burden of their lameness? 

HER wistful face haunts my dreams 
like the rain at night.  

ONCE we dreamt that we were strangers. 
We wake up to find that we were dear to each other.  

SORROW is hushed into peace in my heart
like the evening among the silent trees."""

"""设字符串s为上面的诗句，写下程序语句以实现："""

'''(1)计算s中所有单词的数目，以变量n_word表示'''
n_word = 0
#在下面写自己的程序
n_word = len(s.replace('\n',' ').split())
#在上面写自己的程序
print(f'所有单词的数目为：{n_word}')


'''(2)计算s中所有非空的行的数目，以变量n_line表示'''
n_line = 0
#在下面写自己的程序
n_line = len([line for line in s.split('\n') if line.strip() != ''])
#在上面写自己的程序
print(f'所有非空的行的数目为：{n_line}')


'''(3)使用字符串的format()方法输出n_word和n_line：格式要求每个数字占6个格'''
#在下面写自己的程序
print("n_word:{:6d}\nn_line:{:6d}".format(n_word, n_line))
#在上面写自己的程序

#=================================作业题2========================================
'''设字符串s为上一页的诗句，写下程序语句和运行结果：'''

'''(1)将s中所有的标点符号(,.?)替换为空字符，然后以/将s中所有非空的行连起来，并输出所生成字符串的前50个字符'''

#在下面写自己的程序
temp = s.replace(',','').replace('.','').replace('?','')
lines = [line.strip() for line in temp.split('\n') if line.strip()]
result = '/'.join(lines)
print(result[:50])
#在上面写自己的程序


'''(2)将s中所有的标点符号(,.?)替换为空字符，然后以,将s中所有的单词连起来，并输出所生成字符串的前50个字符'''

#在下面写自己的程序
temp = s.replace(',','').replace('.','').replace('?','')
words = temp.split()
result = ','.join(words)
print(result[:50])
#在上面写自己的程序




