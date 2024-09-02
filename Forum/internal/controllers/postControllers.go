// controllers/post_controller.go
package controllers

import (
	"Forum/internal/models"
	"Forum/internal/services"
	"Forum/internal/utils"
	"errors"
	"fmt"
	"strconv"

	"github.com/gin-gonic/gin"
)

//AuthMiddleware 中间件（应该）返回了Username 和 Type(两个Int)，还有UUID, string

// 发布帖子
type CreatData struct {
	Author  string `json:"UUID" binding:"required"`    // 一个常规字符串字段
	Content string `json:"content" binding:"required"` // 一个指向字符串的指针, allowing for null values
}

func PostCreate(c *gin.Context) {
	//获取用户信息、获取帖子内容
	var data CreatData
	err := c.ShouldBindJSON(&data)
	if err != nil {
		utils.JsonErrorResponse(c, 500, "Failed to parse JSON")
	}

	//校验发帖权限
	flag, _ := services.CheckUserByUUID(data.Author)
	if !flag {
		utils.JsonUnAuthorizedResponse(c, "请检查权限！")
	} else {
		//校验通过，保存到数据库，又要先补全了
		err = services.CreatePost(models.Post{
			Author:  data.Author,
			Content: data.Content,
			Status:  false,
		})
		if err == nil {
			utils.JsonSuccessResponse(c, nil)
		} else {
			utils.JsonErrorResponse(c, 500, "Failed to add specific post in DataBase as a interal error occurred.")
		}
	}

}

// 获取成功发布的帖子列表
func GetPosts(c *gin.Context) {
	Posts, err := services.GetPosts()
	if err != nil {
		utils.JsonErrorResponse(c, 500, "Something went wrong while fetching the posts from the database.")
	} else {
		if len(Posts) == 0 { //如果还没有人发
			c.JSON(200, gin.H{"code": 200, "data": "Narf!还没有人发帖子呢，赶紧来一条吧~", "msg": "success"})
		} else {
			// slicejson, _ := json.Marshal(Posts)
			// unquotedString, _ := strconv.Unquote(slicejson)
			c.JSON(200, gin.H{
				"code": 200,
				"data": gin.H{"post_list": Posts},
				"msg":  "success",
			})
		}
	}
}

// 修改帖子
type EditData struct {
	Author  string `json:"UUID" binding:"required"`    //用户UUID（本人）
	ID      uint   `json:"post_id" binding:"required"` //帖子ID
	Content string `json:"content" binding:"required"` //新的内容
}

func PostEdit(c *gin.Context) {
	var EditData EditData
	err := c.ShouldBindJSON(&EditData)
	if err != nil {
		utils.JsonErrorResponse(c, 500, "Failed to parse JSON")
	}
	//检验权限 本人是否有权限 这帖子是不是本人发的
	flag1, _ := services.CheckUserByUUID(EditData.Author)
	flag2, _ := services.CheckPostByUUID(EditData.ID, EditData.Author)
	if flag1 && flag2 {
		services.UpdatePost(EditData.ID, EditData.Content)
		if err == nil {
			utils.JsonSuccessResponse(c, nil)
		} else {
			utils.JsonInternalServerErrorResponse(c, "Oops! Something went wrong while updating the post in the database.")
		}
	} else {
		utils.JsonUnAuthorizedResponse(c, "请检查权限！")
	}
}

// 删除帖子
type DeleteData struct {
	Author string //用户UUID（本人）
	ID     uint   //帖子的ID
}

func PostDelete(c *gin.Context) {
	var DeletePost EditData
	DeletePost.Author = c.Query("UUID")
	gap, _ := strconv.ParseUint(c.Query("post_id"), 10, 64)
	DeletePost.ID = uint(gap)

	//检验权限 本人是否有权限 这帖子是不是本人发的 或者管理员删帖（预留接口）
	flag1, _ := services.CheckUserByUUID(DeletePost.Author)
	flag2, _ := services.CheckPostByUUID(DeletePost.ID, DeletePost.Author)
	flag3 := services.IsAdmin(DeletePost.Author)
	if flag1 && flag2 || flag3 {
		err := services.DeletPost(DeletePost.ID, DeletePost.Author)
		if err == nil {
			utils.JsonSuccessResponse(c, nil)
		} else {
			utils.JsonInternalServerErrorResponse(c, "Oops! Something went wrong while deleting the post in the database.")
		}
	} else {
		utils.JsonUnAuthorizedResponse(c, "Unauthorized access!")
	}
}

// 批复举报，approve=1批准，删帖，approve=2不准，marked改回。
func PostReportApprove(postID uint, approval int, cUUID string) error {
	switch approval {
	case 1:
		err := services.DeletPost(postID, cUUID) //批准，不走PostDelet方法，直接删帖
		if err != nil {
			return err
		}
		break
	case 2:
		err := services.RemarkPost(postID, cUUID) //不准，改marked为1
		if err != nil {
			return err
		}
		break
	default:
		return errors.New("unknown approval method")
	}
	return nil
}

// 审批新发的帖子
func HandelNewPost(postID uint, approval int, cUUID string) {
	switch approval {
	case 1: //通过
		services.RemarkPost(postID, cUUID)
		break
	case 2: //不通过
		services.DisapprovePost(postID, cUUID)
		break
	default:
		err := errors.New("unknown approval method") //预留接口
		fmt.Printf("log.Logger: %v\n", err)
		break
	}
}
