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
export declare class fpreader {
    private wrapped;
    public enroll_stages: number;
    public supports_imaging: boolean;
    public supports_identification: boolean;
    public img_width: number;
    public img_height: number;
    public close: () => void;
    public start_enroll: (callback: (err: any, result: fp_enroll_result, fpdata: Buffer, fpimage: Buffer, height: Number, width: Number) => void) => void;
    public stop_enroll: (callback: () => void) => void;
    public start_identify: (callback: (err: any, success: any) => void) => void;
    public stop_identify: (callback: () => void) => void;
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
