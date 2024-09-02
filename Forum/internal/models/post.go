package models

import "gorm.io/gorm"

type Post struct {
	gorm.Model
	Author  string `json:"AuthorUUID"` //发布者的UUID
	Content string `json:"Content"`    //内容
	Status  bool   `json:"Status"`     //是否可见（取决于审核）
	Admin   string `json:"AadminUUID"` //上次已修改的(审核者)的UUID
	Marked  int    `json:"Marked"`     // 被标记的次数,0未审核，1审核正常通过，剩下的自然数-1=被标记次数
}
