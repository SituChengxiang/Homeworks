package middleware

import (
	"bytes"
	"fmt"
	"github.com/gin-gonic/gin"
)

// 自定义一个结构体，实现 gin.ResponseWriter interface
type responseWriter struct {
	gin.ResponseWriter
	b *bytes.Buffer
}

// 重写 Write([]byte) (int, error) 方法
func (w responseWriter) Write(b []byte) (int, error) {
	//向一个bytes.buffer中写一份数据来为获取body使用
	w.b.Write(b)
	//完成gin.Context.Writer.Write()原有功能
	return w.ResponseWriter.Write(b)
}

func PrintResponse(c *gin.Context) {
	writer := responseWriter{
		c.Writer,
		bytes.NewBuffer([]byte{}),
	}
	c.Writer = writer

	c.Next()

	fmt.Println("response body：" + writer.b.String())
}
