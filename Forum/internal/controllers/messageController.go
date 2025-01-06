package controllers

import (
	"Forum/internal/database"
	"Forum/internal/services"
	"Forum/internal/utils"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

//AuthMiddleware 中间件（应该）返回了Username 和 Type(两个Int)，还有UUID, string

// 举报
type ReportData struct {
	PostID int    `json:"post_id" binding:"required"` // 帖子编号
	Reason string `json:"reason" binding:"required"`  // 举报原因
	UserID string `json:"UUID" binding:"required"`    // 用户编号(使用者本人)
}

func ReportPost(c *gin.Context) {
	var reportData ReportData
	err := c.ShouldBindJSON(&reportData)
	if err != nil {
		utils.JsonErrorResponse(c, 500, "Failed to parse JSON")
	}
	//校验用户登录信息
	err = services.CheckUserExistByUUID(reportData.UserID)
	if err != nil { //登录信息无效直接返回
		utils.JsonUnAuthorizedResponse(c, err.Error())
	} else {
		//校验帖子是否存在
		err = services.CheckPostExistByID(reportData.PostID)
		if err == nil { //如果存在，则进行举报
			err1 := services.ReportPost(reportData.PostID, reportData.UserID, reportData.Reason)
			err2 := services.MarkPost(reportData.PostID)
			if err1 == nil && err2 == nil {
				utils.JsonSuccessResponse(c, nil)
			} else {
				utils.JsonErrorResponse(c, 500, "Failed to report post")
			}
		} else { // 不存在或者其他错误直接未授权的访问
			utils.JsonUnAuthorizedResponse(c, "Unauthorized changes!")
		}
	}
}

// 获取待批复的帖子（管理员）
func GetPendingApproval(c *gin.Context) {
	if !services.IsAdmin(c.Query("UUID")) { //服务端校验用户类型
		utils.JsonUnAuthorizedResponse(c, "Invalid identity information!")
	} else {
		resultsReport, err1 := services.GetUnprocessedMessages() //获取所有未处理的举报
		resultsNew, err2 := services.GetNewPostMessages()        //获取所有未处理的新增帖子
		if err1 != nil && err2 != nil {
			utils.JsonInternalServerErrorResponse(c, err1.Error())
		}
		if len(resultsReport)+len(resultsNew) == 0 {
			utils.JsonResponse(c, 200, 200, "Narf!你已经干完活啦~休息一下吧(￣y▽,￣)╭ ", nil)
		} else {
			c.JSON(200, gin.H{
				"code": 200,
				"data": gin.H{
					"report_list": resultsReport,
					"new_list":    resultsNew,
				},
				"msg": "success"})
		}
	}
}

// 处理帖子的状态（管理员）
type HandleIdea struct {
	UUID     string `json:"UUID" binding:"required"`     // 用户UUID(本人)
	PostID   uint   `json:"post_id" binding:"required"`  // 帖子编号
	Approval int    `json:"approval" binding:"required"` // 是否通过1过2不过
}

// 处理举报
func HandleReport(c *gin.Context) {
	var handleIdea HandleIdea
	err := c.ShouldBindJSON(&handleIdea)
	if err != nil {
		utils.JsonErrorResponse(c, 500, "Failed to parse JSON")
		panic(err)
	}

	switch handleIdea.Approval {
	case 1, 2:
		if services.IsAdmin(handleIdea.UUID) {
			// 对messages和posts两个表同时进行操作
			err1 := PostReportApprove(handleIdea.PostID, handleIdea.Approval, handleIdea.UUID)
			err2 := MessageReportApprove(handleIdea.PostID, handleIdea.Approval, handleIdea.UUID)
			if err1 == nil && err2 == nil {
				utils.JsonResponse(c, 200, 200, "string", nil)
			} else {
				log.Logger.Error()
				utils.JsonResponse(c, 200, 500, "Invalid request", nil)
			}
		} else {
			utils.JsonUnAuthorizedResponse(c, "Invalid identity information!")
		}
		break
	default:
		utils.JsonErrorResponse(c, 400, "Invalid approval method")
	}
}

// 对message的操作
func MessageReportApprove(postID uint, approval int, cUUID string) error {
	err := services.UpdateMessageStatus(postID, approval)
	if err != nil {
		return err
	}
	return nil
}

// 获取审批结果
func GetApprovalResult(c *gin.Context) {
	db := database.GetDB() // 获取数据库连接
	response, err := services.GetReportedPosts(db, c.Query("UUID"))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to retrieve messages"})
		return
	} else {
		c.JSON(200, gin.H{"code": 200, "data": gin.H{"report_list": response}, "msg": "success"})
	}
}
