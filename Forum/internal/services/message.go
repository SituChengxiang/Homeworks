package services

import (
	"Forum/internal/database"
	"Forum/internal/models"
	"os"
	"time"

	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
	"gorm.io/gorm"
	"gorm.io/gorm/clause"
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

// 举报帖子/发送消息
func ReportPost(postID int, cUUID, reason string) error {
	var ReportMsg models.Message
	var CurrentUser models.User
	ReportMsg.Content, ReportMsg.CreatedAt, ReportMsg.Status = reason, time.Now(), 0
	post := uint(postID)
	Search := database.DB.First(&CurrentUser, "uuid = ?", cUUID)

	// 判断查询是否成功
	if Search.Error != nil {
		log.Logger.Error()
		panic(Search.Error)
	}
	ReportMsg.PostID = post
	ReportMsg.Sender = CurrentUser
	result := database.DB.Create(&ReportMsg)
	return result.Error
}

// 获取未处理的举报信息
type Result struct {
	SenderUsername string `json:"username"`
	PostContent    string `json:"content"`
	Reason         string `json:"reason"`
	PostID         uint   `json:"post_id"`
}

func GetUnprocessedMessages() ([]Result, error) {
	var messages []models.Message
	var results []Result

	err := database.DB.Preload("Sender").Preload("Post", "status = ? AND marked > ?", true, 1).Where("messages.status = ?", 0).Find(&messages).Error
	if err != nil {
		log.Logger.Err(err).Msg("GetUnprocessedMessages")
		return nil, err
	}

	for _, msg := range messages {
		results = append(results, Result{
			SenderUsername: msg.Sender.Username,
			PostContent:    msg.Post.Content,
			Reason:         msg.Content, // 假设 Content 字段表示举报原因
			PostID:         msg.PostID,
		})
	}
	log.Logger.Info().Interface("result", results).Msg("GetUnprocessedMessages")
	return results, nil
}

// 获取未审批的新帖子
type NewPost struct {
	PostContent string `json:"content"`
	PostID      uint   `json:"post_id"`
}

func GetNewPostMessages() ([]NewPost, error) {
	var posts []models.Post
	var resultsNew []NewPost

	err := database.DB.Where("status = ? AND marked = ? AND admin = ?", 0, 0, "").Find(&posts).Error
	if err != nil {
		log.Error()
		return nil, err
	}

	for _, pts := range posts {
		resultsNew = append(resultsNew, NewPost{
			PostContent: pts.Content,
			PostID:      pts.ID,
		})
	}
	log.Logger.Info().Interface("result", resultsNew).Msg("GetNewPostMessages")
	return resultsNew, nil
}

// 消息status更新，1表示已处理同意，2表示已处理拒绝
func UpdateMessageStatus(postID uint, status int) error {
	//初始化日志
	logFile, err := os.OpenFile("./log/log.log", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0666)
	if err != nil {
		log.Error().Err(err).Msg("Failed to open log file")
		return err
	}
	defer logFile.Close()

	multi := zerolog.MultiLevelWriter(os.Stdout, logFile)
	logger := zerolog.New(multi).With().Timestamp().Logger()
	// 开始事务
	tx := database.DB.Begin()
	defer func() {
		if r := recover(); r != nil {
			tx.Rollback()
			logger.Error().Msgf("Transaction rollback due to panic: %v", r)
		}
	}()
	//加锁UPDATE
	if err := tx.Clauses(clause.Locking{Strength: "UPDATE"}).Where("post_id = ?", postID).First(&models.Message{}).Error; err != nil {
		tx.Rollback()
		logger.Error().Err(err).Msg("Failed to lock post row")
		return err
	}
	// 更新messages表中post_id=postID的status
	if err := tx.Model(&models.Message{}).Where("post_id = ?", postID).Updates(models.Message{Status: status}).Error; err != nil {
		tx.Rollback()
		logger.Error().Err(err).Msg("Failed to update post status and marked")
		return err
	}

	if err := tx.Commit().Error; err != nil {
		logger.Error().Err(err).Msg("Failed to commit transaction")
		return err
	}

	return nil
}

// 获取举报审批状态
type ReportResult struct {
	PostID      uint   `json:"post_id"`
	PostContent string `json:"content"`
	Reason      string `json:"reason"`
	Status      int    `json:"status"`
}

func GetReportedPosts(db *gorm.DB, uuid string) ([]ReportResult, error) {
	var messages []models.Message
	var Response []ReportResult

	if uuid != "" {
		var user models.User
		if err := db.Where("uuid = ?", uuid).First(&user).Error; err != nil {
			log.Error().Err(err).Msg("failed to find user")
			return nil, err
		}

		if err := db.Where("sender_id = ?", user.ID).Find(&messages).Error; err != nil {
			log.Error().Err(err).Msg("failed to find messages")
			return nil, err
		}
	} else {
		if err := db.Limit(10).Find(&messages).Error; err != nil {
			log.Error().Err(err).Msg("failed to find messages")
			return nil, err
		}
	}

	for _, msg := range messages {
		Response = append(Response, ReportResult{
			PostID:      msg.PostID,
			PostContent: msg.Post.Content,
			Reason:      msg.Content,
			Status:      msg.Status,
		})
	}

	log.Info().Msg("messages retrieved successfully")
	return Response, nil
}
