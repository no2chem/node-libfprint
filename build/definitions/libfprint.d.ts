/// <reference path="../typings/tsd.d.ts" />
export declare enum fp_enroll_result {
    ENROLL_COMPLETE = 1,
    ENROLL_FAIL = 2,
    ENROLL_PASS = 3,
    ENROLL_RETRY = 100,
    ENROLL_RETRY_TOO_SHORT = 101,
    ENROLL_RETRY_CENTER_FINGER = 102,
    ENROLL_RETRY_REMOVE_FINGER = 103,
}
export declare class fpreader {
    private wrapped;
    enroll_stages: number;
    supports_imaging: boolean;
    supports_identification: boolean;
    img_width: number;
    img_height: number;
    close: () => void;
    start_enroll: (callback: (err: any, result: fp_enroll_result, fpdata: Buffer, fpimage: Buffer, height: Number, width: Number) => void) => void;
    stop_enroll: () => void;
    start_identify: (callback: (err: any, success: any) => void) => void;
    stop_identify: () => void;
    constructor(fpinstance: any);
}
export declare class fprint {
    init(): number;
    discover(): any[];
    get_reader(handle: number): fpreader;
    exit(): void;
    constructor();
}
