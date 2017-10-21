#include "app.h"
#include "DD_Gene.h"
#include "DD_RCDefinition.h"
#include "SystemTaskManager.h"
#include <stdlib.h>
#include "message.h"
#include "MW_GPIO.h"
#include "MW_IWDG.h"
#include "MW_flash.h"
#include "constManager.h"
#include "trapezoid_ctrl.h"

/*suspensionSystem*/
static
int suspensionSystem(void);

static
int armSystem(void);

static
int windlassRotate(void);

/* 秘密道具移動用の変数 */
int push_v_limitsw = 0;
int push_s_limitsw = 0;

/*メモ
 *g_ab_h...ABのハンドラ
 *g_md_h...MDのハンドラ
 *
 *g_rc_data...RCのデータ
 */

#define WRITE_ADDR (const void*)(0x8000000+0x400*(128-1))/*128[KiB]*/
flashError_t checkFlashWrite(void){
  const char data[]="HelloWorld!!TestDatas!!!\n"
    "however you like this microcomputer, you don`t be kind to this computer.";
  return MW_flashWrite(data,WRITE_ADDR,sizeof(data));
}

int appInit(void){
  message("msg","hell");

  /* switch(checkFlashWrite()){ */
  /* case MW_FLASH_OK: */
  /*   message("msg","FLASH WRITE TEST SUCCESS\n%s",(const char*)WRITE_ADDR); */
  /*   break; */
  /* case MW_FLASH_LOCK_FAILURE: */
  /*   message("err","FLASH WRITE TEST LOCK FAILURE\n"); */
  /*   break; */
  /* case MW_FLASH_UNLOCK_FAILURE: */
  /*   message("err","FLASH WRITE TEST UNLOCK FAILURE\n"); */
  /*   break; */
  /* case MW_FLASH_ERASE_VERIFY_FAILURE: */
  /*   message("err","FLASH ERASE VERIFY FAILURE\n"); */
  /*   break; */
  /* case MW_FLASH_ERASE_FAILURE: */
  /*   message("err","FLASH ERASE FAILURE\n"); */
  /*   break; */
  /* case MW_FLASH_WRITE_VERIFY_FAILURE: */
  /*   message("err","FLASH WRITE TEST VERIFY FAILURE\n"); */
  /*   break; */
  /* case MW_FLASH_WRITE_FAILURE: */
  /*   message("err","FLASH WRITE TEST FAILURE\n"); */
  /*   break;         */
  /* default: */
  /*   message("err","FLASH WRITE TEST UNKNOWN FAILURE\n"); */
  /*   break; */
  /* } */
  /* flush(); */

  ad_init();

  message("msg","plz confirm\n%d\n",g_adjust.rightadjust.value);

  /*GPIO の設定などでMW,GPIOではHALを叩く*/
  return EXIT_SUCCESS;
}

/*application tasks*/
int appTask(void){
  int ret=0;

  if(__RC_ISPRESSED_R1(g_rc_data)&&__RC_ISPRESSED_R2(g_rc_data)&&
     __RC_ISPRESSED_L1(g_rc_data)&&__RC_ISPRESSED_L2(g_rc_data)){
    while(__RC_ISPRESSED_R1(g_rc_data)||__RC_ISPRESSED_R2(g_rc_data)||
	  __RC_ISPRESSED_L1(g_rc_data)||__RC_ISPRESSED_L2(g_rc_data));
    ad_main();
  }
  
  /*それぞれの機構ごとに処理をする*/
  /*途中必ず定数回で終了すること。*/
  ret = suspensionSystem();
  if(ret){
    return ret;
  }

  ret = armSystem();
  if(ret){
    return ret;
  }
  
  ret = windlassRotate();
  if(ret){
    return ret;
  }

  return EXIT_SUCCESS;
}


/*プライベート 足回りシステム*/
static
int suspensionSystem(void){
  const int num_of_motor = 2;/*モータの個数*/
  int rc_analogdata;/*アナログデータ*/
  unsigned int idx;/*インデックス*/
  int i;

  const tc_const_t tc ={
    .inc_con = 100,
    .dec_con = 225,
  };

  /*for each motor*/
  for(i=0;i<num_of_motor;i++){
    /*それぞれの差分*/
    switch(i){
    case 0:
      idx = MECHA1_MD0;
      rc_analogdata = -DD_RCGetRY(g_rc_data);
      break;
    case 1:
      idx = MECHA1_MD1;
      rc_analogdata = DD_RCGetLY(g_rc_data);
      break;     
   
    default:

      return EXIT_FAILURE;
    }
   
    trapezoidCtrl(-rc_analogdata * MD_GAIN,&g_md_h[idx],&tc);
  }

  return EXIT_SUCCESS;
}

/* 腕振り */
static
int armSystem(void){
  const tc_const_t arm_tcon ={
    .inc_con = 350,
    .dec_con = 350
  };

  /* 腕振り部のduty */
  int arm_target = 0;
  const int arm_up_duty = MD_ARM_UP_DUTY;
  const int arm_down_duty = MD_ARM_DOWN_DUTY;

  /* コントローラのボタンは押されてるか */
  if(__RC_ISPRESSED_R1(g_rc_data)){
    arm_target = arm_up_duty;
    trapezoidCtrl(arm_target,&g_md_h[MECHA1_MD2],&arm_tcon);
  }else if(__RC_ISPRESSED_R2(g_rc_data)){
    arm_target = arm_down_duty;
    trapezoidCtrl(arm_target,&g_md_h[MECHA1_MD2],&arm_tcon);
  }else{
    arm_target = 0;
    trapezoidCtrl(arm_target,&g_md_h[MECHA1_MD2],&arm_tcon);
  }

  return EXIT_SUCCESS;
}

/* 秘密道具移動部 */
static
int windlassRotate(void){
  const tc_const_t windlass_tcon ={
    .inc_con = 250,
    .dec_con = 1000
  };

  /* 秘密道具移動部のduty */
  int windlass_target = 0;
  const int up_duty = MD_UP_DUTY;
  const int down_duty = MD_DOWN_DUTY;
  const int back_duty = MD_BACK_DUTY;
  const int front_duty = MD_FRONT_DUTY;

  /* コントローラのボタンは押されてるか */
  if(__RC_ISPRESSED_TRIANGLE(g_rc_data)){
    windlass_target = up_duty;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD3],&windlass_tcon);
  }else if(push_v_limitsw==1 && __RC_ISPRESSED_CIRCLE(g_rc_data)){
    windlass_target = back_duty;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD4],&windlass_tcon);
  }else if((__RC_ISPRESSED_L1(g_rc_data)) && (__RC_ISPRESSED_SQARE(g_rc_data))){
    windlass_target = front_duty;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD4],&windlass_tcon);
    if(push_v_limitsw){
      windlass_target = front_duty;
      trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD4],&windlass_tcon);
    }else if(push_s_limitsw){
      windlass_target = front_duty;
      trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD4],&windlass_tcon);
    }
  }else if((__RC_ISPRESSED_L1(g_rc_data)) && (__RC_ISPRESSED_CROSS(g_rc_data))){
    windlass_target = down_duty;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD3],&windlass_tcon);
    if(push_s_limitsw){
      windlass_target = down_duty;
      trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD3],&windlass_tcon);
    }
  }else{
    windlass_target = 0;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD3],&windlass_tcon);
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD4],&windlass_tcon);
  }
  
  /* リミットスイッチは押されてるか */
  if(_IS_PRESSED_VERTICAL_LIMITSW()){
    windlass_target = 0;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD3],&windlass_tcon);
    push_v_limitsw = 1;
  }else if(_IS_PRESSED_SIDE_LIMITSW()){
    windlass_target = 0;
    trapezoidCtrl(windlass_target,&g_md_h[MECHA1_MD4],&windlass_tcon);
    push_s_limitsw = 1;
  }
   
  return EXIT_SUCCESS;
}
