COMPOSE   ?= docker-compose
SERVICE   ?= mosquitto
CONTAINER ?= clauduino-mqtt
TOPIC     ?= clauduino/led/status
MSG       ?= task_complete

.PHONY: help up down restart ps logs sub pub smoke clean trigger-stop trigger-subagent trigger-notify

help: ## Show this help
	@awk 'BEGIN {FS = ":.*?## "; printf "Targets:\n"} /^[a-zA-Z_-]+:.*?## / { printf "  \033[36m%-8s\033[0m %s\n", $$1, $$2 }' $(MAKEFILE_LIST)

up: ## Start MQTT broker
	$(COMPOSE) up -d

down: ## Stop MQTT broker
	$(COMPOSE) down

restart: ## Restart broker
	$(COMPOSE) restart $(SERVICE)

ps: ## Show container status
	$(COMPOSE) ps

logs: ## Tail broker logs
	$(COMPOSE) logs -f $(SERVICE)

sub: ## Subscribe to clauduino/# (Ctrl-C to exit)
	docker exec -it $(CONTAINER) mosquitto_sub -h localhost -t 'clauduino/#' -v

pub: ## Publish: make pub TOPIC=... MSG=...
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t '$(TOPIC)' -m '$(MSG)'

smoke: ## End-to-end test: external pub -> broker -> subscriber
	@echo "==> smoke: external pub -> broker -> subscriber"
	@{ \
	  docker exec $(CONTAINER) mosquitto_sub -h localhost -t '$(TOPIC)' -C 1 -W 10 -v > /tmp/clauduino-smoke.out & \
	  SUB_PID=$$!; \
	  sleep 1; \
	  docker run --rm eclipse-mosquitto:2.0 mosquitto_pub -h host.docker.internal -p 1883 -t '$(TOPIC)' -m '$(MSG)'; \
	  wait $$SUB_PID; \
	  echo "received: $$(cat /tmp/clauduino-smoke.out)"; \
	  rm -f /tmp/clauduino-smoke.out; \
	}

trigger-stop: ## Simulate a Claude Stop event
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t 'clauduino/led/stop' -m ''

trigger-subagent: ## Simulate a Claude SubagentStop event
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t 'clauduino/led/subagent_stop' -m ''

trigger-notify: ## Simulate a Claude Notification event
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t 'clauduino/led/notification' -m ''

clean: ## Stop broker and wipe runtime data/log
	$(COMPOSE) down
	rm -rf mosquitto/data/* mosquitto/log/*
