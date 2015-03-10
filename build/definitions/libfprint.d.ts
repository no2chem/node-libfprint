export declare enum fp_enroll_result {
    ENROLL_COMPLETE = 1,
    ENROLL_FAIL = 2,
    ENROLL_PASS = 3,
    ENROLL_RETRY = 100,
    ENROLL_RETRY_TOO_SHORT = 101,
    ENROLL_RETRY_CENTER_FINGER = 102,
    ENROLL_RETRY_REMOVE_FINGER = 103,
    ENROLL_CANCELLED = 200,
}
export declare enum fp_verify_result {
    VERIFY_NO_MATCH = 0,
    VERIFY_MATCH = 1,
    VERIFY_RETRY = 100,
    VERIFY_RETRY_TOO_SHORT = 101,
    VERIFY_RETRY_CENTER_FINGER = 102,
    VERIFY_RETRY_REMOVE_FINGER = 103,
    VERIFY_CANCELLED = 200,
}
export declare enum fp_stop_result {
    STOP_SUCCESS = 1,
    STOP_FAIL = 2,
    STOP_IGNORE = 3,
}
export declare class fpreader {
    private wrapped;
    public enroll_stages: number;
    public supports_imaging: boolean;
    public supports_identification: boolean;
    public img_width: number;
    public img_height: number;
    public close: () => void;
    public update_database: (fplist: string[]) => void;
    public start_enroll: (callback: (err: any, result: fp_enroll_result, fpdata: Buffer, fpimage: Buffer, height: Number, width: Number) => void) => void;
    public stop_enroll: (callback: (err: any, result: any) => void) => void;
    public start_identify: (callback: (err: any, result: fp_verify_result, fpindex: Number, fpimage: Buffer, height: Number, width: Number) => void) => void;
    public stop_identify: (callback: (err: any, result: any) => void) => void;
    public handle_events: () => void;
    constructor(fpinstance: any);
}
export declare class fprint {
    public init(): number;
    public discover(): any[];
    public get_reader(handle: number): fpreader;
    public exit(): void;
    constructor();
}
