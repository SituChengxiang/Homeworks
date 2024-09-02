package middleware

//jwt和AuthMiddleware的代码是从CSDN搬过来的，自己改过

import (
	"Forum/internal/database"
	"Forum/internal/models"
	"Forum/internal/utils"
	"strings"

	"github.com/gin-gonic/gin"
)

func AuthMiddleware() gin.HandlerFunc {

	return func(ctx *gin.Context) {

		// 获取authorization header
		tokenString := ctx.GetHeader("Authorization")

		// validate token formate
		if tokenString == "" || !strings.HasPrefix(tokenString, "Bearer ") {
			utils.JsonUnAuthorizedResponse(ctx, "权限不足")
			//ctx.JSON(http.StatusUnauthorized, gin.H{"code": 401, "message": "权限不足"});	ctx.Abort()
			return
		}

		//提取token的有效部分（"Bearer "共占7位)
		tokenString = tokenString[7:]

		token, claims, err := utils.ParseToken(tokenString)
		if err != nil || !token.Valid {
			// ctx.JSON(http.StatusUnauthorized, gin.H{"code": 401, "message": "权限不足"});ctx.Abort()
			utils.JsonUnAuthorizedResponse(ctx, "权限不足")
			return
		}

		// 验证通过后获取 claim 中的Uername
		username := claims.Username
		DB := database.GetDB()
		var user models.User

		// 用户不存在
		if err := DB.First(&user, username).Error; err != nil {
			//ctx.JSON(http.StatusUnauthorized, gin.H{"code": 401, "message": "权限不足"});ctx.Abort()
			utils.JsonUnAuthorizedResponse(ctx, "权限不足")
			return
		}

		// 用户存在将user的信息写入上下文，方便读取
		ctx.Set("user", user)

		ctx.Next()
	}
}
