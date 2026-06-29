#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update() {
	glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e) {
	//todo: improve this so that if two keys are pressed it doesnt just stop the character
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_w) {
			velocity.z = -1;
		}
		if (e.key.keysym.sym == SDLK_s) {
			velocity.z = 1;
		}
		if (e.key.keysym.sym == SDLK_a) {
			velocity.x = -1;
		}
		if (e.key.keysym.sym == SDLK_d) {
			velocity.x = 1;
		}
	}

	if (e.type == SDL_KEYUP) {
		if (e.key.keysym.sym == SDLK_w) {
			velocity.z = 0;
		}
		if (e.key.keysym.sym == SDLK_s) {
			velocity.z = 0;
		}
		if (e.key.keysym.sym == SDLK_a) {
			velocity.x = 0;
		}
		if (e.key.keysym.sym == SDLK_d) {
			velocity.x = 0;
		}
	}

	if (e.type == SDL_MOUSEMOTION) {
		yaw += (float)e.motion.xrel / 200.f;
		pitch -= (float)e.motion.yrel / 200.f;
	}
}