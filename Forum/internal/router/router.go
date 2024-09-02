package router

import (
	"Forum/internal/controllers"
	"Forum/internal/middleware"

	"github.com/gin-gonic/gin"
)

func Init(r *gin.Engine) {
	const pre = "/api/user"

	apiUni := r.Group(pre)
	{
		apiUni.POST("/login", controllers.Login)
		apiUni.POST("/reg", controllers.Register)
		apiUni.GET("/info", middleware.AuthMiddleware(), controllers.Info)
	}
	const std = "/api/student"

	apiStd := r.Group(std)
	{
		// apiStd.POST("/post", middleware.AuthMiddleware(), controllers.PostCreate)
		apiStd.POST("/post", controllers.PostCreate)
		apiStd.GET("/post", controllers.GetPosts)
		apiStd.PUT("/post", controllers.PostEdit)
		apiStd.DELETE("/post", controllers.PostDelete)
		apiStd.POST("/report-post", controllers.ReportPost)
		apiStd.GET("/report-post", controllers.GetApprovalResult)
	}
	apiAdm := r.Group("/api/admin")
	{
		apiAdm.GET("/report", controllers.GetPendingApproval)
		apiAdm.POST("/report", controllers.HandleReport)
	}
}
