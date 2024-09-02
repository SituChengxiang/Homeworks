package models

import "gorm.io/gorm"

// Message
type Message struct {
	gorm.Model
	Content  string `json:"content"`
	PostID   uint   `gorm:"foreignKey:PostID;references:ID"`
	SenderID uint   `gorm:"foreignKey:UserID;references:ID"` // 假设 User 表的主键是 ID
	Sender   User   `gorm:"foreignKey:SenderID"`
	Status   int    `json:"status"`
	Post     Post   `gorm:"foreignKey:PostID;associationForeignKey:ID"`
	// Reciver  string `json:"ReciverUUID"` 预留私信功能接口
}
