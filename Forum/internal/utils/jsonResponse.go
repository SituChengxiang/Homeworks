package utils

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// 基本的响应模板
func JsonResponse(c *gin.Context, httpStatusCode int, code int, msg string, data interface{}) {
	c.JSON(httpStatusCode, gin.H{
		"code": code,
		"msg":  msg,
		"data": data,
	})
}

// 正常响应
func JsonSuccessResponse(c *gin.Context, data interface{}) {
	JsonResponse(c, http.StatusOK, 200, "success", data)
}

// 服务器返回了一些奇怪的错误
func JsonErrorResponse(c *gin.Context, code int, msg string) {
	JsonResponse(c, http.StatusInternalServerError, code, msg, nil)
}

// 数据内部发生了一些奇怪的错误
func JsonInternalServerErrorResponse(c *gin.Context, msg string) {
	JsonResponse(c, http.StatusInternalServerError, 200500, msg, nil)
}

// 未授权的访问
func JsonUnAuthorizedResponse(c *gin.Context, msg string) {
	JsonResponse(c, http.StatusUnauthorized, 401, msg, nil)
	c.Abort() // 终止后续处理
}
