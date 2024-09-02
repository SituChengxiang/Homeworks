// internal/database/migrate.go
package database

import (
	"Forum/internal/models"
	"os"

	"github.com/rs/zerolog/log"

	"gorm.io/gorm"
)

func autoMigrate(db *gorm.DB) error {
	err := db.AutoMigrate(&models.User{}, &models.Post{}, &models.Message{})

	file, _ := os.OpenFile("./log/log.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	defer file.Close()
	if err != nil {
		log.Logger = log.Output(file)
	}
	logger := log.With().Str("module", "database").Logger()
	logger.Error().Err(err).Msg("An error occurred in Database")
	return nil
}
