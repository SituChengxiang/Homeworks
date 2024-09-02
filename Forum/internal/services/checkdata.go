package services

import (
	"errors"
	"strconv"
)

func CheckUserNameValid(Username string) error {
	//这里写校验输入的username的逻辑
	getint, err := strconv.Atoi(Username)
	if err != nil {
		err := errors.New("invalid username detected")
		return err
	} else if getint < 100000000000 || getint > 999999999999 {
		err := errors.New("invalid username detected")
		return err
	} //至于为什么有长度限制，因为工地的学号长是12位
	return nil
}

func CheckPasswordValid(Password string) error {
	//这里写校验输入的password的逻辑
	if len(Password) < 8 || len(Password) > 16 {
		err := errors.New("invalid password detected")
		return err
	}
	return nil
}

func CheckUserTypeValid(Type int) error {
	//这里写校验输入的type的逻辑
	switch Type {
	case 1, 2:
		return nil
	default:
		err := errors.New("invalid type detected")
		return err
	}
}
