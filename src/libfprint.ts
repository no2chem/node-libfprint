/// <reference path="../typings/tsd.d.ts"/>

var binary = require('node-pre-gyp');
var path = require('path');
var PACKAGE_JSON = path.join(__dirname, '../package.json');
var binding_path = binary.find(path.resolve(PACKAGE_JSON));
var fprintbinding = require(binding_path);

var stream = require('stream');
var util = require('util');
var bunyan = require('bunyan');
var events = require('events');

var log;

export enum fp_enroll_result
{
    ENROLL_COMPLETE = 1,
    ENROLL_FAIL = 2,
    ENROLL_PASS = 3,
    ENROLL_RETRY = 100,
    ENROLL_RETRY_TOO_SHORT = 101,
    ENROLL_RETRY_CENTER_FINGER = 102,
    ENROLL_RETRY_REMOVE_FINGER = 103
}

export class fpreader {
    private wrapped;

    enroll_stages : number;
    supports_imaging : boolean;
    supports_identification: boolean;
    img_width: number;
    img_height: number;

    close = () => {
        this.wrapped.close();
    }

    start_enroll = (callback : (err, result : fp_enroll_result, fpdata : Buffer, fpimage: Buffer, height : Number, width : Number) => void) : void => {
        if (!this.wrapped.enroll_finger(
                function (result: fp_enroll_result, fpdata, fpimage, height : number, width: number)
                {
                    var err = null;
                    if (result != fp_enroll_result.ENROLL_COMPLETE)
                    {
                        err = fp_enroll_result[result];
                    }
                    if (fpdata !== null && fpdata !== undefined)
                    {
                        var data = new Buffer(fpdata.length);
                        fpdata.copy(data);
                    }
                    var image = new Buffer(fpimage.length);
                    fpimage.copy(image);
                    callback(err, result, data, image, height, width);
                }
                ))
        {
            callback("Enroll in progress!", null, null, null, null, null);
        }
    }

    constructor(fpinstance) {
        this.wrapped = fpinstance;

        //these values are static so we can grab them now
        this.enroll_stages = fpinstance.enroll_stages;
        this.supports_imaging = fpinstance.supports_imaging;
        this.supports_identification = fpinstance.supports_identification;
        this.img_width = fpinstance.img_width;
        this.img_height = fpinstance.img_height;
    }
}

export class fprint {

    // Initializes libfprint and returns 0 if successful.
    init() : number  {
        return fprintbinding.init();
    }

    discover() {
        var devices = [];
        fprintbinding.discover( function(handle, devid, drvtype, drvname, drvfullname)
                {
                    var thisdev = {
                        handle: handle,
                        deviceid: devid,
                        driver_type: drvtype,
                        driver: drvname,
                        driver_detail: drvfullname
                    };

                    devices.push(thisdev);
                });

        return devices;
    }

    get_reader(handle: number) {
        var reader = new fprintbinding.fpreader(handle);
        if (typeof reader != 'undefined')
        {
            return new fpreader(reader);
        }
        return null;
    }

    exit() : void {
        return fprintbinding.exit();
    }

    constructor () { }
}
