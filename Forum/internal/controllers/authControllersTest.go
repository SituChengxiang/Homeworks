package controllers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestRegister(t *testing.T) {
	// 初始化 Gin 路由
	r := gin.Default()

	// 注册路由
	r.POST("/register", Register)

	// 创建测试用例
	testCases := []struct {
		name          string
		input         RegisterData
		expectedCode  int
		expectedError string
	}{
		{
			name: "成功注册",
			input: RegisterData{
				Username:   "testuser",
				Password:   "password123",
				RePassword: "password123",
				Name:       "Test User",
				UserType:   1,
			},
			expectedCode: 200,
		},
		{
			name: "两次密码不一致",
			input: RegisterData{
				Username:   "testuser",
				Password:   "password123",
				RePassword: "password456",
				Name:       "Test User",
				UserType:   1,
			},
			expectedCode:  20050301,
			expectedError: "两次密码不一致",
		},
		// 添加更多测试用例...
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			// 将输入数据转换为 JSON
			inputJSON, err := json.Marshal(tc.input)
			if err != nil {
				t.Errorf("无法将输入数据转换为 JSON: %v", err)
			}

			// 创建 HTTP 请求
			req, err := http.NewRequest("POST", "/register", bytes.NewBuffer(inputJSON))
			if err != nil {
				t.Errorf("无法创建 HTTP 请求: %v", err)
			}

			// 创建响应记录器
			respRecorder := httptest.NewRecorder()

			// 处理请求
			r.ServeHTTP(respRecorder, req)

			// 检查响应状态码
			assert.Equal(t, tc.expectedCode, respRecorder.Code)

			// 如果有预期的错误消息，检查响应体是否包含该消息
			if tc.expectedError != "" {
				assert.Contains(t, respRecorder.Body.String(), tc.expectedError)
			}
		})
	}
}

func TestLogin(t *testing.T) {
	// 初始化 Gin 路由
	r := gin.Default()

	// 注册路由
	r.POST("/login", Login)

	// 创建测试用例
	testCases := []struct {
		name          string
		input         LoginData
		expectedCode  int
		expectedError string
	}{
		{
			name: "成功登录",
			input: LoginData{
				Username: "testuser",
				Password: "password123",
			},
			expectedCode: 200,
		},
		{
			name: "用户不存在",
			input: LoginData{
				Username: "nonexistentuser",
				Password: "password123",
			},
			expectedCode:  200506,
			expectedError: "用户不存在",
		},
		// 添加更多测试用例...
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			// 将输入数据转换为 JSON
			inputJSON, err := json.Marshal(tc.input)
			if err != nil {
				t.Errorf("无法将输入数据转换为 JSON: %v", err)
			}

			// 创建 HTTP 请求
			req, err := http.NewRequest("POST", "/login", bytes.NewBuffer(inputJSON))
			if err != nil {
				t.Errorf("无法创建 HTTP 请求: %v", err)
			}

			// 创建响应记录器
			respRecorder := httptest.NewRecorder()

			// 处理请求
			r.ServeHTTP(respRecorder, req)

			// 检查响应状态码
			assert.Equal(t, tc.expectedCode, respRecorder.Code)

			// 如果有预期的错误消息，检查响应体是否包含该消息
			if tc.expectedError != "" {
				assert.Contains(t, respRecorder.Body.String(), tc.expectedError)
			}
		})
	}
}
