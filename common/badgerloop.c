#include <string.h>
#include "badgerloop.h"
#include "hal/stm32f7xx_hal.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "ethernetif.h"

/*****************************************************************************/
/*                                 Data Buffer                               */
/*****************************************************************************/
uint8_t telemetry_buffer[47];

/* Team ID, Status */
uint8_t *team_id = &telemetry_buffer[0];
uint8_t *status = &telemetry_buffer[1];

/* Position, Velocity, Acceleration */
int *acceleration = (int *) &telemetry_buffer[2],
	*position = (int *) &telemetry_buffer[6],
	*velocity = (int *) &telemetry_buffer[10],

	/* Battery Voltage and Current*/
	*v_batt = (int *) &telemetry_buffer[14],
	*i_batt = (int *) &telemetry_buffer[18],

	/* Battery Temperature, Pod Temperature */
	*t_batt = (int *) &telemetry_buffer[22],
	*t_pod = (int *) &telemetry_buffer[26];

/* Strip Count */
uint32_t *s_count = (uint32_t *) &telemetry_buffer[30];

/*====================== Dashboard Only Fields ==============================*/

/* Ambient Pressure */
uint16_t *p_amb = (uint16_t *) &telemetry_buffer[34],

/* Pressure Sensors */
	*pr_p1 = (uint16_t *) &telemetry_buffer[36],
	*pr_p2 = (uint16_t *) &telemetry_buffer[38],
	*br_p1 = (uint16_t *) &telemetry_buffer[40],
	*br_p2 = (uint16_t *) &telemetry_buffer[42],
	*br_p3 = (uint16_t *) &telemetry_buffer[44];

/* Limit Swtich States */
uint8_t *lim_states = &telemetry_buffer[46];
/*****************************************************************************/
/*****************************************************************************/

/* Networking */
ip_addr_t to_spacex, to_dashboard;
struct udp_pcb *udp_spacex, *udp_dashboard;
struct pbuf *spacex_payload, *dashboard_payload;

uint32_t last_telem_timestamp;
static err_t lwip_error = ERR_OK;

/* for accepting commands from the dashboard */
void udp_echo_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
	const ip_addr_t *dst_ip, u16_t dst_port) {
	printf("got a packet from %"U16_F".%"U16_F".%"U16_F".%"U16_F" on port %d\r\n",
		ip4_addr1_16(dst_ip), ip4_addr2_16(dst_ip),
		ip4_addr3_16(dst_ip), ip4_addr4_16(dst_ip), dst_port);
	printf("contents: %s\r\n", (char *) p->payload);
}

int badgerloop_init(void) {

	IP4_ADDR(&to_spacex, 192, 168, 0, DEV_IP);
	IP4_ADDR(&to_dashboard, 192, 168, 0, DEV_IP);

	udp_spacex = udp_new();
	udp_dashboard = udp_new();

	if (udp_spacex == NULL || udp_dashboard == NULL) return -1;

	lwip_error = udp_connect(udp_spacex, &to_spacex, 3000);
	if (lwip_error != ERR_OK) return -1;

	lwip_error = udp_connect(udp_dashboard, &to_dashboard, 3000);
	if (lwip_error != ERR_OK) return -1;

	lwip_error = udp_bind(udp_dashboard, &(gnetif.ip_addr), 3000);
	if (lwip_error != ERR_OK) return -1;

	udp_recv(udp_dashboard, udp_echo_recv, NULL);

	/* default values */
	*team_id = 3; // TEAM_ID
	*status = FAULT; // IDLE

	/* for endianness testing */
	SET_ACCEL(1);
	SET_POS(-4);
	SET_VEL(-1);
	SET_VBATT(-3);
	SET_IBATT(2);
	SET_TBATT(-5);
	SET_TPOD(-2);
	SET_SCOUNT(4);

	return 0;
}

int send_telemetry_to_SpaceX(void) {

	spacex_payload = pbuf_alloc(PBUF_TRANSPORT, 34, PBUF_RAM);
	if (spacex_payload == NULL) return -1;

	memcpy(spacex_payload->payload, telemetry_buffer, 34);

	last_telem_timestamp = ticks;

	lwip_error = udp_send(udp_spacex, spacex_payload);
	if (lwip_error != ERR_OK) return -1;

	pbuf_free(spacex_payload);

	return 0;
}

int send_telemetry_to_Dashboard(void) {

	dashboard_payload = pbuf_alloc(PBUF_TRANSPORT, 48, PBUF_RAM);
	if (dashboard_payload == NULL) return -1;

	memcpy(dashboard_payload->payload, telemetry_buffer, 48);

	last_telem_timestamp = ticks;

	lwip_error = udp_send(udp_dashboard, dashboard_payload);
	if (lwip_error != ERR_OK) return -1;

	pbuf_free(dashboard_payload);

	return 0;
}

