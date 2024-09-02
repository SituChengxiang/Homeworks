package services

import (
	"Forum/internal/database"
	"Forum/internal/models"
	"Forum/internal/utils"
	"os"

	_ "github.com/go-sql-driver/mysql"
	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
	"gorm.io/gorm"
	"gorm.io/gorm/clause"
)

// 创建帖子
func CreatePost(newpost models.Post) error {
	result := database.DB.Create(&newpost)
	return result.Error
}

// 获取所有发布的已经审核的帖子
type tinyPost struct {
	ID        uint   `json:"id"`
	Content   string `json:"content"`
	Author    string `json:"uuid"`
	CreatedAt string `json:"created_at"`
}

func GetPosts() ([]tinyPost, error) {
	var posts []tinyPost
	err := database.DB.Order("-id").Model(&models.Post{}).Find(&tinyPost{}).Find(&posts, "status = ?", 1).Error
	if err != nil {
		return nil, err
	}
	return posts, nil
}

// 根据UUID检查帖子是不是自己发的
func CheckPostByUUID(postId uint, cUUID string) (bool, error) {
	var postInDB models.Post
	err := database.DB.First(&postInDB, "id = ?", postId).Error
	if err != nil {
		return false, err
	}
	return postInDB.Author == cUUID, nil
}

// 修改帖子
func UpdatePost(ID uint, Content string) error {
	// // 开始事务
	tx := database.DB.Begin()
	defer func() {
		if r := recover(); r != nil {
			tx.Rollback()
		} else if err := tx.Commit(); err != nil {
			tx.Rollback()
		}
	}()
	//修改
	var editedPost models.Post
	result := database.DB.Model(&editedPost).Where("id = ?", ID).Updates(models.Post{Content: Content})
	return result.Error
}

// 删除帖子 查找 加密content 软删除
func DeletPost(ID uint, cUUID string) error {
	var DelPost models.Post
	database.DB.Where("id = ?", ID).First(&DelPost)
	if DelPost.Content == "" { //内容为空直接删除
		result := database.DB.Where("id = ? ", ID).Delete(&models.Post{})
		return result.Error
	} else {
		HashedContent, _ := utils.EncryptAES(DelPost.Content)                                                      //先加密
		database.DB.Model(&DelPost).Where("id = ?", ID).Updates(models.Post{Content: HashedContent, Admin: cUUID}) //再更新，保留操作人员信息
		result := database.DB.Where("id = ?", ID).Delete(&models.Post{})                                           //软删除
		return result.Error                                                                                        //可以使用GORM的Unscoped方法来恢复软删除的记录
	}
}

// 检查帖子是否存在
func CheckPostExistByID(postID int) error {
	result := database.DB.Where("id = ?", postID).First(&models.Post{})
	return result.Error
}

// 帖子marked+1（举报的一部分）
func MarkPost(postID int) error {
	var ProblemPost models.Post
	result := database.DB.Model(&ProblemPost).Where("id = ?", postID).Update("marked", gorm.Expr("marked + ?", 1))
	return result.Error
}

// 帖子remark回归1 （举报审批不通过/新增审批通过）
func RemarkPost(postID uint, uuid string) error {
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
	if err := tx.Clauses(clause.Locking{Strength: "FOR UPDATE"}).Where("id = ?", postID).First(&models.Post{}).Error; err != nil {
		tx.Rollback()
		logger.Error().Err(err).Msg("Failed to lock post row")
		return err
	}
	// 更新posts表中ID=postID的marked
	if err := tx.Model(&models.Post{}).Where("id = ?", postID).Updates(models.Post{Marked: 1, Admin: uuid}).Error; err != nil {
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

func DisapprovePost(ID uint, UUID string) error {
	// // 开始事务
	tx := database.DB.Begin()
	defer func() {
		if r := recover(); r != nil {
			tx.Rollback()
		} else if err := tx.Commit(); err != nil {
			tx.Rollback()
		}
	}()
	//修改
	var editedPost models.Post
	result := database.DB.Model(&editedPost).Where("id = ?", ID).Updates(models.Post{Admin: UUID, Marked: 0})
	return result.Error
}
