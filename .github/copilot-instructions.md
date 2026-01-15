# Copilot Instructions for Homeworks Workspace

This workspace is a collection of distinct projects and homework assignments in C, Go, Matlab, and Python. Identify the active sub-project based on the file path and apply the corresponding conventions.

## 🐹 Go Projects (Forum & StdServe)

Both projects use **Gin** (Web), **Gorm** (ORM), **Viper** (Config), and **Zerolog** (Logging).

### 1. Forum (`Forum/`)
- **Architecture**: Follows Standard Go Project Layout with `internal/` directory.
  - `internal/controllers`: Handle HTTP requests.
  - `internal/services`: Business logic.
  - `internal/models`: Data structures.
  - `internal/utils`: Helpers (Crypto, JWT, JSON response).
- **Authentication**: Uses legacy `github.com/dgrijalva/jwt-go`.
- **Logging**: Uses `github.com/rs/zerolog/log` directly.
- **Database**: MySQL/MariaDB via Gorm.

### 2. StdServe Back-end (`stdserve-Back_end/Back-end/`)
- **Architecture**: Flat structure (no `internal/`).
  - `controllers/`, `services/`, `models/` reside at the project root.
- **Authentication**: Uses modern `github.com/golang-jwt/jwt/v5`.
- **Logging**: Use the helper `utils.LogError(err)` defined in `utils/logger.go`.
- **Configuration**: `config.yaml` loaded via Viper.

### 🛡️ General Go Patterns
- **Layered Design**: Controllers should parse requests and call Services. Services handle logic and call DB/Models.
- **Error Handling**: Return JSON responses with error messages. Use `utils.JsonResponse` (or similar) if available.
- **Config**: Do not hardcode secrets. Refer to `config.yaml` keys.

## 🧪 C Experiments (`C-exp/`)
- **Context**: Single-file C programs for algorithms and experiments.
- **Style**: Standard C99/C11.
- **Build**: No global makefile. Compile individual files (e.g., `gcc filename.c -o filename`).

## 🔢 Matlab & Python (`matlab/`, `python/`)
- **Context**: Numerical analysis, physics simulations, and course designs.
- **Matlab**:
  - `CourseDesign/`: Structured project with `main.m` and helper functions.
  - Prefer vectorized operations over loops where possible.
- **Python**:
  - Mixed scripts (`.py`) and notebooks (`.ipynb`).
  - Libraries: `numpy`, `matplotlib`, `scipy`.
  - `VSMethematicalPhysicsMethod/`: Specific physics simulation project.

## 🚀 Workflow
- **Context Detection**: Always check the file path to determine if you are in `Forum`, `stdserve`, or a standalone script folder.
- **Go Modules**: Be aware of the different JWT versions between the two Go projects.
