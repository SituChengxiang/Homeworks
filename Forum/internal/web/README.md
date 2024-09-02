# 关于这个本来不应该出现的文件夹

这是一个萌新在不知道怎么用 apifox 的时候拿非常简单的代码测试接口的文件夹，所以里面有很多非常简单的代码，并且没有经过任何优化，你甚至可以看到很多对于html文件的调用。

-------

早期不知道怎么写出的登陆调用接口

```go
func Register(w http.ResponseWriter, r *http.Request) {
 if r.Method == "POST" {
  // 解析表单数据
  err := r.ParseForm()
  if err != nil {
   // 处理错误
   log.Println("表单解析错误:", err)
   http.Error(w, "表单解析错误", http.StatusInternalServerError)
   return
  }

  // 获取表单元素
  username := r.Form.Get("username")
  password := r.Form.Get("password")

  // 打印表单元素
  log.Println("Username:", username)
  log.Println("Password:", password)

  // 从Viper中获取数据库配置
  dsn := viper.GetString("database.dsn")

  // 建立数据库连接
  db, err := sql.Open("mysql", dsn)
  if err != nil {
   // 处理错误
   log.Println("数据库连接失败:", err)
   http.Error(w, "数据库连接失败", http.StatusInternalServerError)
   return
  } else {
   // 检查用户名是否已经存在
   var count int
   err = db.QueryRow("SELECT COUNT(*) FROM users WHERE username = ?", username).Scan(&count)
   if err != nil {
    // 处理错误
    log.Println("查询用户名失败:", err)
    http.Error(w, "查询用户名失败", http.StatusInternalServerError)
    return
   }

   if count > 0 {
    // 用户名已经存在，返回错误
    log.Println("重复的用户名")
    http.Error(w, "用户已经存在，请联系管理员处理！", http.StatusBadRequest)
    return
   }

   // 添加新的用户
   _, err = db.Exec("INSERT INTO users (username, password) VALUES (?, ?)", username, password)
   if err != nil {
    // 处理错误
    log.Println("添加用户失败:", err)
    http.Error(w, "添加用户失败", http.StatusInternalServerError)
    return
   } else {
    log.Println("添加用户成功")
    fmt.Fprintf(w, "注册成功")
   }
  }
  defer db.Close()

 }
}
```
