#!/bin/sh
docker compose up build-ran-base --build
docker compose up build-softmodems --pull=never
docker compose up build-nr-ue-image build-gnb-image build-lte-ru-image build-lte-ue-image build-enb-image --pull=never --build
