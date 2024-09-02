package models

import "gorm.io/gorm"

// User 用户
type User struct {
	gorm.Model        // 包含ID、CreatedAt、UpdatedAt、DeletedAt字段，预留补充账户信息、更新账户信息记录的接口
	Username   string `json:"username"`
	Password   string `json:"-"`
	Type       int    `json:"user_type"`
	Status     bool   `json:"status"`
	Name       string `json:"name"`
	UUID       string `json:"user_id"`
}
