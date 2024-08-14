/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <stdlib.h>


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */

#define NUNCHUK_NODE DT_ALIAS(nunchuk)

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);




#define WHITE 65535

#define TILE 10
#define SIZE ((240 / TILE) * (240 / TILE))

struct coord {
	char x;
	char y;
};

struct queue {
	struct coord tab[SIZE];
	int front;
	int back;
};


uint16_t screen[240*240] = {WHITE};

struct queue snake = {.front=0,.back=0};

const int speed = 4;

int dir_x = TILE;
int dir_y = 0;


uint16_t color(int blue, int green, int red) {
	return (blue & 31) | ((green & 63) << 5) | ((red & 31) << 11);
}

void set_buffer(uint16_t *colors) {
	for (int i = 0;i<240;i++) {
		for (int j=0;j<240;j++){
			colors[i*240 + j] = color(31,63,31);
		}
	}
}


void draw_rect(uint16_t *screen,int x, int y, int height, int width, uint16_t color) {
	for (int i=0;i<height;i++) {
		for (int j=0;j<width;j++) {
			screen[y+j + (x+i) * 240] = color;
		}
	}
}

void handler(struct input_event *evt, void *user_data) {
	if (evt->type != INPUT_EV_ABS) {
		return;
	}

	int coef = 0;
	if (evt->value < 10) {
		coef = -1;
	} else if (evt->value > 240) {
		coef = 1;
	}

	if (coef) {
		dir_x = coef * evt->code * TILE * -1;
		dir_y = coef * !evt->code * TILE;
	}

	printf("%d %d\n",dir_x,dir_y);
}


void push(struct queue *q, char x, char y) {
	q->tab[q->front].x = x;
	q->tab[q->front].y = y;
	q->front = (q->front + 1) % SIZE;
}

struct coord get(struct queue *q) {
	if (q->front == q->back) {
		printf("Error: queue empty\n");
		struct coord res = {.x = 0, .y = 0};
		return res;
	}
	struct coord ret = q->tab[q->back];
	q->back = (q->back + 1) % SIZE;
	return ret;
}

int check_loose(struct queue *snake, int x, int y) {
	if (x > (240 - TILE) || y > (240 - TILE) || x < 0 || y < 0) {
		return 1;
	}
	if (snake->front > snake->back) {
		for (int i=snake->back; i < snake->front;i++) {
			if (x == snake->tab[i].x && y == snake->tab[i].y) {
				return 1;
			}
		}
	} else {
		for (int i=snake->back; i<SIZE;i++) {
			if (x == snake->tab[i].x && y == snake->tab[i].y) {
				return 1;
			}
		}
		for (int i=0; i < snake->front; i++) {
			if (x == snake->tab[i].x && y == snake->tab[i].y) {
				return 1;
			}
		}
	}
	return 0;
}

void play(const struct device *display_dev) {
	struct display_buffer_descriptor buffer_d;

	buffer_d.buf_size = 240 * 240;
	buffer_d.width = 240;
	buffer_d.height = 240;
	buffer_d.pitch = 240;

	printf("%d\n",screen[150]);

	set_buffer(screen);

	printf("%d\n",screen[150]);

	display_blanking_off(display_dev);

	
	int target_x = rand()/(RAND_MAX / (240 / TILE)) * TILE;
	int target_y = rand()/(RAND_MAX / (240 / TILE)) * TILE;
	printf("target: %d %d\n",target_x, target_y);

	draw_rect(screen,target_x,target_y,TILE,TILE,color(0,0,31));


	display_write(display_dev, 0, 0, &buffer_d, &screen);

	snake.front = 0;
	snake.back = 0;


	int x = (100 / TILE) * TILE;
	int y = (100 / TILE) * TILE;

	push(&snake, x, y);
	int score = 0;


	int speed = 200;

	printf("start\n");
	while (1) {


		x += dir_x;
		y += dir_y;
		if (check_loose(&snake, x,y)) {
			printf("GAME OVER\n");
			printf("SCORE: %d\n",score);
			break;
		}
		push(&snake, x, y);

		draw_rect(screen,x,y,TILE,TILE,0);


		printf("%d %d\n",x,y);

		if (x != target_x || y != target_y) {
			struct coord res = get(&snake);
			draw_rect(screen, res.x,res.y,TILE,TILE,WHITE);
		} else {
			score++;
			target_x = rand()/(RAND_MAX / (240 / TILE)) * TILE;
			target_y = rand()/(RAND_MAX / (240 / TILE)) * TILE;

			printf("new target: %d %d\n",target_x,target_y);

			if (speed > 100) {
				speed -= 10;
			} else if (speed > 50) {
				speed -= 5;
			} else if (speed > 20) {
				speed -= 1;
			}
		}

		draw_rect(screen,target_x,target_y,TILE,TILE,color(0,0,31));




		display_write(display_dev, 0, 0, &buffer_d, &screen);


		k_msleep(speed);
		gpio_pin_toggle_dt(&led);
	}

}


#ifdef CONFIG_INPUT
int main(void) {
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	const struct device *display_dev;



	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display_dev)) {
		printf("display not ready\n");
		return 1;
	}

	srand(0);
	INPUT_CALLBACK_DEFINE(NULL,handler,NULL);

	while (1) {
		play(display_dev);
		k_msleep(1000);
	}
}
#else


void worker(struct k_work *work) {
	printf("test worker\n");
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	k_work_reschedule(dwork, K_MSEC(1000));
}

int main(void)
{
// 	const struct device *display_dev;

// 	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

// 	if (!device_is_ready(display_dev)) {
// 		printf("display not ready\n");
// 		return 1;
// 	}

	// const struct device *nunchuk = DEVICE_DT_GET(NUNCHUK_NODE);

	// printf("%s\n",nunchuk->name);

	struct i2c_dt_spec i2c = I2C_DT_SPEC_GET(NUNCHUK_NODE);

	const struct device *bus = DEVICE_DT_GET(DT_NODELABEL(i2c1));

	printf("address: %d\n",i2c.addr);

	uint8_t data[4] = {0xf0,0x55,0xfb,0x00};
	// i2c_write_dt(&i2c,data,3);
	int ret = i2c_write_dt(&i2c,(uint8_t *)&data,2);
	if (ret < 0) printf("Error\n");
	k_msleep(1);
	ret = i2c_write_dt(&i2c,&data[2],2);
	if (ret < 0) printf("Error 2\n");
	k_msleep(1);

	// struct k_work_delayable work;
	// k_work_init_delayable(&work,worker);

	// ret = k_work_reschedule(&work, K_MSEC(1000));



	uint8_t buf[6] = {0,1,2,3,4,5};
	while (1) {
		uint8_t a = 0;
		// i2c_write_dt(&i2c,&a,1);
		// k_msleep(10);
		// ret = i2c_read_dt(&i2c,&buf,6);
		if (ret < 0) printf("Error 3\n");
		// for (int i=0;i<6;i++) {
		// 	printf("%d ",buf[i]);
		// }
		// printf("\n");

		printf("Z : %d\n",buf[5] & 1);
		printf("C : %d\n",buf[5] & 2);
		printf("X joystick: %d\n",buf[0]);
		printf("Y joystick: %d\n",buf[1]);

		k_msleep(53);
	}



// 	// struct display_buffer_descriptor buffer_d;

// 	// buffer_d.buf_size = 256 * 256;
// 	// buffer_d.width = 256;
// 	// buffer_d.height = 256;
// 	// buffer_d.pitch = 256;

// 	// set_buffer(colors);

// 	// display_blanking_off(display_dev);

// 	// display_write(display_dev, 0, 0, &buffer_d, &colors);



	return 0;
}

#endif