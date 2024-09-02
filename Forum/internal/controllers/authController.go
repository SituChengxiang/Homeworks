package controllers

import (
	"Forum/internal/models"
	"Forum/internal/services"
	"Forum/internal/utils"
	"fmt"
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
	_ "github.com/go-sql-driver/mysql"
	"github.com/google/uuid"
	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

func InitLogger() {
	// 创建日志文件
	file, err := os.OpenFile("Forum/log/AuthController.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if err != nil {
		fmt.Println("无法打开日志文件:", err)
		return
	}
	defer file.Close()

	// 设置日志输出到文件
	log.Logger = log.Output(zerolog.ConsoleWriter{Out: file})

	// 设置日志级别
	log.Logger = log.Logger.Level(zerolog.InfoLevel)
}

type RegisterData struct {
	Username   string `json:"username" binding:"required"`
	Password   string `json:"password" binding:"required"`
	RePassword string `json:"repassword" binding:"required"`
	Name       string `json:"name" binding:"required"`
	UserType   int    `json:"user_type" binding:"required"`
}

// 注册
func Register(c *gin.Context) {
	// 接收参数
	var data RegisterData
	err := c.ShouldBindJSON(&data)
	if err != nil {
		utils.JsonErrorResponse(c, 500, "Failed to parse JSON")
	}
	// 接收参数后
	if data.Password != data.RePassword {
		utils.JsonErrorResponse(c, 20050301, "两次密码不一致")
		return
	} //1. 判断两次密码是否一致

	err = services.CheckUserNameValid(data.Username)
	if err != nil {
		utils.JsonErrorResponse(c, 200502, "用户名必须为纯数字，且长度为12位")
		return
	} //2. 判断用户名是否有效

	err = services.CheckPasswordValid(data.Password)
	if err != nil {
		utils.JsonErrorResponse(c, 200503, "密码长度必须在8-16位")
		return
	} // 3.判断密码长度是否合理

	err = services.CheckUserTypeValid(data.UserType)
	if err != nil {
		utils.JsonErrorResponse(c, 200504, "用户类型不合法")
		return
	} //4. 判断用户类型是否合法

	err = services.CheckUserExistByUserName(data.Username)
	if err == nil {
		utils.JsonErrorResponse(c, 200505, "用户(名)已存在")
		return
	} else if err == gorm.ErrRecordNotFound {
		//确认没有后再进行下一步操作,通过新建username对象补全strut

		hasedPassword, err := bcrypt.GenerateFromPassword([]byte(data.Password), bcrypt.DefaultCost)
		if err != nil {
			utils.JsonErrorResponse(c, 500, "failed to encrypt password")
			return
		}

		uuid := uuid.New().String() //新建uuid是因为我没想好怎么合理地生成用户id，再去数据库拉一遍？Gorm学的不是很明白

		err = services.AddUser(models.User{
			Username: data.Username,
			Password: string(hasedPassword),
			Type:     data.UserType,
			Status:   true,
			Name:     data.Name,
			UUID:     uuid,
		})
		if err == nil {
			utils.JsonSuccessResponse(c, nil)
		} else {
			utils.JsonErrorResponse(c, 500, "Failed to add specific user in DataBase as a interal error occurred.")
		}
	}
}

type LoginData struct {
	Username string `json:"username" binding:"required"`
	Password string `json:"password" binding:"required"`
}

// 登录
func Login(c *gin.Context) {
	//接收参数
	var data LoginData
	err := c.ShouldBindJSON(&data)
	if err != nil {
		// c.JSON(500, gin.H{"code": 500,"msg":  "Failed to parse JSON","data": nil,});return
		utils.JsonErrorResponse(c, 500, "Failed to parse JSON")
	}
	//接受参数成功后判断用户是否存在
	err = services.CheckUserExistByUserName(data.Username)
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			// c.JSON(500, gin.H{"code": 200506,"data": nil,"msg":  "用户不存在",});return
			utils.JsonErrorResponse(c, 200506, "用户不存在")
		} else {
			// c.JSON(500, gin.H{"code": 500,"msg":  "Failed to retrieve specific user in DataBase","data": nil,});return
			utils.JsonInternalServerErrorResponse(c, "Failed to retrieve specific user in DataBase")
			return
		}
	} else {
		//过关之后获取用户信息
		var user *models.User
		// 在数据库中先找到用户
		user, err = services.GetUserByUserName(data.Username)
		if err != nil {
			//c.JSON(500, gin.H{"code": 200500,"msg":  "Failed to retrieve specific user in DataBase","data": nil,});return
			utils.JsonErrorResponse(c, 200500, "Failed to retrieve specific user in DataBase")
		}

		//检测密码是否正确
		flag := services.CheckPassword(data.Password, user.Password)
		if !flag {
			utils.JsonErrorResponse(c, 200507, "密码错误")
			return
		}

		// 发token
		token, err := utils.ReleaseToken(*user)
		if err != nil {
			utils.JsonInternalServerErrorResponse(c, "Failed to generate token")
			//记录下错误
			log.Printf("token generate error: %v", err)
			return
		}

		//返回结果
		c.JSON(200, gin.H{"code": 200, "data": gin.H{
			"user_id":   user.UUID,
			"user_type": user.Type,
		}, "msg": "success",
			"token": token})
	}

}

func Info(c *gin.Context) {

	user, _ := c.Get("user")
	//将用户信息返回
	c.JSON(http.StatusOK, gin.H{"data": gin.H{"user": user}})

}
