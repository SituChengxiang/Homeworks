// internal/database/databse.go
package database

import (
	"Forum/internal/config"
	"fmt"
	"log"

	_ "github.com/go-sql-driver/mysql"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

var DB *gorm.DB

func Init() {
	user := config.Config.GetString("database.user")
	pass := config.Config.GetString("database.pass")
	port := config.Config.GetString("database.port")
	host := config.Config.GetString("database.host")
	name := config.Config.GetString("database.name")

	dsn := fmt.Sprintf("%v:%v@tcp(%v:%v)/%v?charset=utf8&parseTime=True&loc=Local", user, pass, host, port, name)
	db, err := gorm.Open(mysql.Open(dsn), &gorm.Config{})

	if err != nil {
		log.Fatal("Database connect failed: ", err)
	}

	// 自动建表（已有不会覆盖）
	err = autoMigrate(db)
	if err != nil {
		log.Fatal("Database migrate failed: ", err)
	}

	DB = db
}

func GetDB() *gorm.DB {
	return DB
}

// func Connect() (*sql.DB, error) {
// 	db, err := sql.Open("mysql", "root:123@tcp(127.0.0.1:3306)/test?parseTime=True&loc=Local&tls=true&autocommit=true&clientFoundRows=true&allowNativePasswords=true&allowAllFiles=true&allowOldPasswords=true&allowPublicKeyRetrieval=true&maxAllowedPacket=0&readTimeout=0&writeTimeout=0&timeout=0&interpolateParams=true&charset=utf8mb4&collation=utf8mb4_unicode_ci&multiStatements=true&sql_mode='ALLOW_INVALID_DATES,STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'")
// 	if err != nil {
// 		return nil, err
// 	}
// 	return db, nil
// }
