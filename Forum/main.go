package main

import (
	"Forum/internal/database"
	"Forum/internal/router"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
)

func InitLogger() {
	logFile, err := os.Create("app.log")
	if err != nil {
		log.Fatal().Err(err).Msg("Failed to create log file")
	}
	zerolog.TimeFieldFormat = zerolog.TimeFormatUnix
	log.Logger = log.Output(zerolog.ConsoleWriter{Out: logFile})
}

func main() {
	database.Init()
	r := gin.Default()
	router.Init(r)

	err := r.Run(":9090")
	if err != nil {
		log.Fatal().Err(err).Msg("Failed to start server")
	}
}
