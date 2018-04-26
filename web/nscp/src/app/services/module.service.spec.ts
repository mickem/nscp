import { TestBed, inject } from '@angular/core/testing';

import { ModuleService } from './module.service';

describe('ModuleService', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [ModuleService]
    });
  });

  it('should be created', inject([ModuleService], (service: ModuleService) => {
    expect(service).toBeTruthy();
  }));
});
