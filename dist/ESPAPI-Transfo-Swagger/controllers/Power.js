'use strict';

var url = require('url');


var Power = require('./PowerService');


module.exports.powerGET = function powerGET (req, res, next) {
  Power.powerGET(req.swagger.params, res, next);
};

module.exports.powerInverseGET = function powerInverseGET (req, res, next) {
  Power.powerInverseGET(req.swagger.params, res, next);
};

module.exports.powerInversePOST = function powerInversePOST (req, res, next) {
  Power.powerInversePOST(req.swagger.params, res, next);
};

module.exports.powerStrengthGET = function powerStrengthGET (req, res, next) {
  Power.powerStrengthGET(req.swagger.params, res, next);
};

module.exports.powerStrengthPOST = function powerStrengthPOST (req, res, next) {
  Power.powerStrengthPOST(req.swagger.params, res, next);
};
