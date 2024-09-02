package services

import (
	"Forum/internal/database"
	"Forum/internal/models"
	"errors"
	"fmt"
	"os"
	"time"

	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

func init() {
	// 创建一个多行日志格式器
	output := zerolog.ConsoleWriter{Out: os.Stdout, TimeFormat: time.RFC3339}

	// 设置日志级别
	zerolog.SetGlobalLevel(zerolog.InfoLevel)

	// 创建一个日志文件，并设置日志格式
	file, err := os.OpenFile("./log/log.log", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		panic(err)
	}

	// 将日志输出到控制台和文件
	log.Logger = log.Output(zerolog.MultiLevelWriter(output, file)).With().Timestamp().Logger()
}

// CheckUserExistByUserName 检索用户名是否存在于数据库中，如果不存在返回 gorm.ErrRecordNotFound，这个时候可以换到注册逻辑中
func CheckUserExistByUserName(username string) error {
	result := database.DB.Where("username = ?", username).First(&models.User{})
	log.Error()
	return result.Error
}

// CheckUserExistByUUID 检索用户名是否存在于数据库中，如果不存在返回 gorm.ErrRecordNotFound，这个时候可以换到注册逻辑中
func CheckUserExistByUUID(UUID string) error {
	result := database.DB.Where("uuid = ?", UUID).First(&models.User{})
	log.Error()
	return result.Error
}

// CheckUserByUUID 检索用户名是否存在于数据库中以及是否被禁用
func CheckUserByUUID(uuid string) (bool, error) {
	var user models.User
	result := database.DB.Where("uuid = ?", uuid).First(&user)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return false, gorm.ErrRecordNotFound
		}
		return false, result.Error
	}
	if !user.Status {
		return false, nil
	}
	return true, nil
}

// GetUserByUserName 根据用户名在数据库中检索用户
func GetUserByUserName(username string) (*models.User, error) {
	var user models.User
	result := database.DB.Where("username = ?", username).First(&user)
	if result.Error != nil {
		return nil, result.Error
	}
	return &user, nil
}

// 比对密码
func CheckPassword(pwdinput string, pwddb string) bool {
	err := bcrypt.CompareHashAndPassword([]byte(pwddb), []byte(pwdinput))
	return err == nil
}

// AddUser 添加用户到数据库
func AddUser(newuser models.User) error {
	result := database.DB.Create(&newuser)
	log.Error()
	return result.Error
}

// ISAdmin 检查用户是否为管理员
func IsAdmin(uuid string) bool {
	var Applyuser models.User
	log.Logger.Info().Msg(fmt.Sprintf("uuid: %s", uuid))
	result := database.DB.Where("uuid = ?", uuid).First(&Applyuser)
	if result.Error != nil {
		log.Error()
		panic(result.Error)
	}
	log.Logger.Info().Bool("is_admin", Applyuser.Type == 2).Msg("Is Admin")
	return Applyuser.Type == 2
}
