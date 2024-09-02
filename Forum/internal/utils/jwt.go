package utils

//jwt 鉴权的东西来自

import (
	"Forum/internal/models"
	"log"
	"strconv"
	"time"

	"github.com/dgrijalva/jwt-go"
)

// jwt加密密钥
var jwtKey = []byte("a_secret_crect")

// token的claim
type Claims struct {
	Username uint64
	Type     int
	UUID     string
	jwt.StandardClaims
}

// 发放token
func ReleaseToken(user models.User) (string, error) {
	num, err := strconv.ParseUint(user.Username, 10, 0)
	if err != nil {
		log.Printf("failed to parse user id to uint64, err: %v\n", err) //这都会出错的话自己看看得了ಠ_ಠ
	}
	//token的有效期
	expirationTime := time.Now().Add(7 * 24 * time.Hour)

	claims := &Claims{

		//自定义字段
		Username: num,
		Type:     user.Type,
		UUID:     user.UUID,
		//标准字段
		StandardClaims: jwt.StandardClaims{

			//过期时间
			ExpiresAt: expirationTime.Unix(),
			//发放的时间
			IssuedAt: time.Now().Unix(),
			//发放者
			Issuer: "127.0.0.1",
			//主题
			Subject: "user token",
		},
	}

	//使用jwt密钥生成token
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString(jwtKey)

	if err != nil {
		return "", err
	}

	//返回token
	return tokenString, nil
}

// 从tokenString中解析出claims并返回
func ParseToken(tokenString string) (*jwt.Token, *Claims, error) {
	claims := &Claims{}

	token, err := jwt.ParseWithClaims(tokenString, claims, func(token *jwt.Token) (i interface{}, err error) {
		return jwtKey, nil
	})
	return token, claims, err
}
