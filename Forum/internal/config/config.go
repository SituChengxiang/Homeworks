// internal/config/config.go

package config

import (
	"os"

	"github.com/rs/zerolog"
	"github.com/spf13/viper"
)

var Config = viper.New()

func init() {
	Config.SetConfigName("config")
	Config.SetConfigType("yaml")
	Config.AddConfigPath(".")
	Config.WatchConfig() // 自动将配置读入Config变量

	// 读取配置文件
	err := Config.ReadInConfig()
	if err != nil {
		logger := zerolog.New(os.Stdout).With().Timestamp().Logger()
		logger.Fatal().Msgf("Failed to load config: %s", err)

	}
}
